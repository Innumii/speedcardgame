package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
	swaggerFiles "github.com/swaggo/files"
	ginSwagger "github.com/swaggo/gin-swagger"

	"github.com/FYL-Studios/speedcardgame/auth/controllers"
	_ "github.com/FYL-Studios/speedcardgame/auth/docs"
	"github.com/FYL-Studios/speedcardgame/auth/dtos"
	"github.com/FYL-Studios/speedcardgame/auth/middleware"
	"github.com/FYL-Studios/speedcardgame/auth/models"
	"github.com/FYL-Studios/speedcardgame/auth/repositories"
	"github.com/FYL-Studios/speedcardgame/auth/services"
	"github.com/FYL-Studios/speedcardgame/auth/utils"

	"github.com/redis/go-redis/v9"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

func loadOptionalEnvFiles(paths ...string) {
	for _, path := range paths {
		if info, err := os.Stat(path); err == nil && !info.IsDir() {
			if loadErr := godotenv.Overload(path); loadErr != nil {
				log.Printf("Warning: failed to load %s: %v", path, loadErr)
			}
		}
	}
}

func main() {
	loadOptionalEnvFiles("../.env", "./.env")

	// Construct DSN from environment variables
	dsn := fmt.Sprintf(
		"host=%s user=%s password=%s dbname=%s port=%s sslmode=%s TimeZone=%s",
		os.Getenv("POSTGRES_HOST"),
		os.Getenv("POSTGRES_USER"),
		os.Getenv("POSTGRES_PASSWORD"),
		os.Getenv("POSTGRES_DB"),
		os.Getenv("POSTGRES_PORT"),
		os.Getenv("POSTGRES_SSLMODE"),
		os.Getenv("POSTGRES_TIMEZONE"),
	)
	// fmt.Println(dsn)

	// Database connection setup
	db, err := gorm.Open(postgres.Open(dsn), &gorm.Config{})
	if err != nil {
		log.Fatal("Failed to connect to the database:", err)
	}

	redisAddr := fmt.Sprintf("%s:%s", os.Getenv("REDIS_HOST"), os.Getenv("REDIS_PORT"))
	redisClient := redis.NewClient(&redis.Options{
		Addr: redisAddr, // Redis server address
	})

	ctx := context.Background()
	_, err = redisClient.Ping(ctx).Result()
	if err != nil {
		log.Fatal("Failed to connect to Redis:", err)
	}
	fmt.Println("Connected to Redis successfully")

	// Handle command to drop tables
	if len(os.Args) > 1 {
		command := os.Args[1]

		switch command {
		case "db:drop":
			dropTables(db, redisClient)
			return // Exit after dropping the tables
		case "db:create":
			createTables(db)
			return
		default:
			fmt.Println("Invalid command")
			return // Exit on invalid command
		}
	}

	// Auto migrate user model
	err = db.AutoMigrate(&models.User{})
	if err != nil {
		log.Fatalf("Failed to migrate the database: %v", err)
	}

	// Initialize repositories
	userRepository := repositories.NewGormUserRepository(db)

	// Initialize services and controllers
	sessionService := services.NewSessionService(redisClient)
	authService := services.NewAuthService(userRepository, sessionService)
	authController := controllers.NewAuthController(authService)
	seedDevUsers(authService)
	debugLoggingEnabled := utils.GetEnvAsBool("DEBUG_LOG_ENABLED", false)
	httpRequestLoggingEnabled := utils.GetEnvAsBool("HTTP_REQUEST_LOG_ENABLED", true)
	utils.ConfigureGinMode(debugLoggingEnabled)
	utils.LogStartupConfiguration("auth", debugLoggingEnabled, httpRequestLoggingEnabled)

	// Initialize Gin router
	r := gin.New()
	if httpRequestLoggingEnabled {
		r.Use(utils.GinRequestLogger(debugLoggingEnabled))
	}
	r.Use(gin.Recovery())

	// Allow CORS
	r.Use(cors.New(cors.Config{
		AllowOrigins:     []string{"https://localhost:3000"},
		AllowMethods:     []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowHeaders:     []string{"Origin", "Content-Type", "Authorization", "X-Session-ID", "X-Internal-Key"}, // add X-Session-ID
		AllowCredentials: true,
		MaxAge:           12 * time.Hour,
	}))

	// Public routes — no auth required
	public := r.Group("/auth")
	{
		public.POST("/register", authController.Register)
		public.POST("/login", authController.Login)
		public.POST("/password/reset/request", authController.RequestPasswordReset)
		public.POST("/password/reset/confirm", authController.ConfirmPasswordReset)
	}

	// Protected routes — must be logged in
	protected := r.Group("/auth")
	protected.Use(middleware.RequireAuth(sessionService))
	{
		protected.POST("/logout", authController.Logout)
		protected.GET("/me", authController.GetMe)
		protected.PATCH("/password/change", authController.ChangePassword)
	}

	// Health check endpoint for AWS
	r.GET("/health", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "healthy"})
	})

	//Swagger docs
	r.GET("/swagger/*any", ginSwagger.WrapHandler(swaggerFiles.Handler))

	// Start server (HTTPS only)
	err = r.RunTLS(":8080", "certs/server.crt", "certs/server.key")
	if err != nil {
		log.Fatalf("Failed to start the server: %v", err)
	}
}

func dropTables(db *gorm.DB, redisClient *redis.Client) {

	// Clear all sessions
	clearAllSessions(redisClient)

	// Drop all tables
	if err := db.Migrator().DropTable(&models.User{}); err != nil {
		log.Fatal("Failed to drop tables:", err)
	}
	fmt.Println("Tables dropped successfully")
}

func createTables(db *gorm.DB) {
	// AutoMigrate creates the tables based on the models
	// add models as required here
	if err := db.AutoMigrate(&models.User{}); err != nil {
		log.Fatal("Failed to create tables:", err)
	}
	fmt.Println("Tables created successfully")
}

func clearAllSessions(redisClient *redis.Client) {
	ctx := context.Background()

	_, err := redisClient.FlushAll(ctx).Result()

	if err != nil {
		log.Fatal("Failed to clear Redis store:", err)
	}
}

func seedDevUsers(authService *services.AuthService) {
	devUsers := []dtos.RegisterDTO{
		{Name: "admin", Email: "admin@example.com", Password: "admin"},
		{Name: "test", Email: "test@example.com", Password: "test"},
	}

	const maxAttempts = 8
	const retryDelay = 2 * time.Second

	for _, user := range devUsers {
		for attempt := 1; attempt <= maxAttempts; attempt++ {
			_, err := authService.RegisterDevUser(user)
			if err == nil {
				break
			}

			if strings.Contains(strings.ToLower(err.Error()), "email already taken") {
				break
			}

			if attempt == maxAttempts {
				log.Printf("Dev user seed failed for %s after %d attempts: %v", user.Email, maxAttempts, err)
				break
			}

			log.Printf("Dev user seed retry %d/%d for %s: %v", attempt, maxAttempts, user.Email, err)
			time.Sleep(retryDelay)
		}
	}
}
