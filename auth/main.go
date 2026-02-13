package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"

	"github.com/ryanljk/speedcardgame/auth/controllers"
	"github.com/ryanljk/speedcardgame/auth/docs"
	"github.com/ryanljk/speedcardgame/auth/models"
	"github.com/ryanljk/speedcardgame/auth/repositories"
	"github.com/ryanljk/speedcardgame/auth/services"

	"github.com/redis/go-redis/v9"
	"github.com/swaggo/files"
	"github.com/swaggo/gin-swagger"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

func main() {

	err := godotenv.Load()
	if err != nil {
		log.Fatalf("Error loading .env file")
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

	// Set up Swagger
	setSwagger()

	// Initialize repositories
	userRepository := repositories.NewGormUserRepository(db)

	// Initialize services and controllers
	sessionService := services.NewSessionService(redisClient)
	authService := services.NewAuthService(userRepository, sessionService)
	authController := controllers.NewAuthController(authService)

	// Initialize Gin router
	r := gin.Default()

	// Allow CORS
	r.Use(cors.New(cors.Config{
		AllowOrigins:     []string{"http://localhost:3000"}, // Add your frontend domain
		AllowMethods:     []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowHeaders:     []string{"Origin", "Content-Type", "Authorization"},
		AllowCredentials: true,
		MaxAge:           12 * time.Hour,
	}))

	r.GET("/swagger/*any", ginSwagger.WrapHandler(swaggerFiles.Handler))

	// Auth routes
	r.POST("/register", authController.Register)
	r.POST("/login", authController.Login)
	r.POST("/logout", authController.Logout)
	r.GET("/me", authController.GetMe)
	r.POST("/reset-password", authController.RequestPasswordReset)
	r.POST("/reset-password/confirm", authController.ConfirmPasswordReset)
	r.PATCH("/change-password", authController.ChangePassword)

	// Start server
	err = r.Run(":8080")
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

func setSwagger() {
	docs.SwaggerInfo.Title = "Masala Dosa API"
	docs.SwaggerInfo.Description = "API documentation for the Masala Dosa application"
	docs.SwaggerInfo.Version = "1.0"
	docs.SwaggerInfo.Host = "localhost:8080"
	docs.SwaggerInfo.BasePath = "/"
	docs.SwaggerInfo.Schemes = []string{"http", "https"}
}
