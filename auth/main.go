package main

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
	swaggerFiles "github.com/swaggo/files"
	ginSwagger "github.com/swaggo/gin-swagger"

	"github.com/Ryanljk/speedcardgame/auth/controllers"
	_ "github.com/Ryanljk/speedcardgame/auth/docs"
	"github.com/Ryanljk/speedcardgame/auth/dtos"
	"github.com/Ryanljk/speedcardgame/auth/models"
	"github.com/Ryanljk/speedcardgame/auth/repositories"
	"github.com/Ryanljk/speedcardgame/auth/services"
	"github.com/Ryanljk/speedcardgame/auth/utils"

	"github.com/redis/go-redis/v9"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

func main() {

	err := godotenv.Load(".env", "../.env")
	if err != nil {
		log.Printf("Warning: .env file not found, using existing environment variables")
	}

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
	allowedOrigins := []string{"https://localhost:3000"}
	if configuredOrigins := strings.TrimSpace(os.Getenv("CORS_ALLOWED_ORIGINS")); configuredOrigins != "" {
		parts := strings.Split(configuredOrigins, ",")
		allowedOrigins = allowedOrigins[:0]
		for _, part := range parts {
			origin := strings.TrimSpace(part)
			if origin != "" {
				allowedOrigins = append(allowedOrigins, origin)
			}
		}
	}

	r.Use(cors.New(cors.Config{
		AllowOrigins:     allowedOrigins,
		AllowMethods:     []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowHeaders:     []string{"Origin", "Content-Type", "Authorization"},
		AllowCredentials: true,
		MaxAge:           12 * time.Hour,
	}))
	r.Use(httpsSecurityMiddleware())

	registerAuthRoutes := func(group gin.IRoutes) {
		group.POST("/register", authController.Register)
		group.POST("/login", authController.Login)
		group.POST("/logout", authController.Logout)
	}

	registerAuthRoutes(r.Group("/auth"))

	// Health check endpoint for AWS
	r.GET("/health", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "healthy"})
	})

	//Swagger docs
	r.GET("/swagger/*any", ginSwagger.WrapHandler(swaggerFiles.Handler))

	// Start server with TLS
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

func isHTTPSRequest(c *gin.Context) bool {
	if c.Request != nil && c.Request.TLS != nil {
		return true
	}
	return strings.EqualFold(strings.TrimSpace(c.GetHeader("X-Forwarded-Proto")), "https")
}

func httpsSecurityMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		if !isHTTPSRequest(c) {
			host := strings.TrimSpace(c.Request.Host)
			if host != "" {
				c.Redirect(http.StatusMovedPermanently, "https://"+host+c.Request.URL.RequestURI())
				c.Abort()
				return
			}
		}

		c.Header("Strict-Transport-Security", "max-age=31536000; includeSubDomains")
		c.Next()
	}
}
