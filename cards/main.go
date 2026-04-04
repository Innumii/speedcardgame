package main

import (
	"fmt"
	"log"
	"net/http"
	"os"

	_ "github.com/FYL-Studios/speedcardgame/cards/docs"
	httpSwagger "github.com/swaggo/http-swagger"

	"github.com/FYL-Studios/speedcardgame/cards/config"
	"github.com/FYL-Studios/speedcardgame/cards/models"
	"github.com/FYL-Studios/speedcardgame/cards/services"
	"github.com/FYL-Studios/speedcardgame/cards/util"
	"github.com/go-chi/chi"
	"github.com/go-chi/cors"
	"github.com/joho/godotenv"
)

func resolveImageDir() string {
	candidates := []string{
		"assets/cards",
		"cards/assets/cards",
		"./cards/assets/cards",
		"../cards/assets/cards",
	}

	for _, candidate := range candidates {
		if info, err := os.Stat(candidate); err == nil && info.IsDir() {
			return candidate
		}
	}

	return "assets/cards"
}

func main() {
	if err := godotenv.Load(".env"); err != nil {
		log.Printf(".env not loaded: %v", err)
	}

	debugLoggingEnabled := util.GetEnvAsBool("DEBUG_LOG_ENABLED", false)
	httpRequestLoggingEnabled := util.GetEnvAsBool("HTTP_REQUEST_LOG_ENABLED", true)
	util.LogStartupConfiguration("cards", debugLoggingEnabled, httpRequestLoggingEnabled)

	portString := os.Getenv("PORT") //grab port value from .env
	if portString == "" {
		log.Fatal("PORT not found in env")
	}
	fmt.Println("Port", portString)

	// Connect to PostgreSQL database using GORM
	dsn := os.Getenv("DATABASE_URL") // You can set this in your .env file
	if err := config.InitializeDatabase(dsn); err != nil {
		log.Fatal("Failed to connect to database", err)
	}

	// Migrate models
	if err := config.DB.AutoMigrate(&models.Deck{}, &models.Card{}, &models.Inventory{}, &models.PaymentLedger{}); err != nil {
		log.Fatalf("Error auto-migrating tables: %v", err)
	}

	if err := services.InitializePaymentsFromEnv(); err != nil {
		log.Printf("Payments module disabled: %v", err)
	} else {
		log.Printf("Payments module initialized")
	}

	seedPath := os.Getenv("CARD_CSV_PATH")
	if seedPath == "" {
		seedPath = "cards.csv"
	}
	if err := services.SeedCardsFromCSV(config.DB, seedPath); err != nil {
		if os.IsNotExist(err) {
			log.Printf("Card seed skipped: %s not found", seedPath)
		} else {
			log.Printf("Card seed failed: %v", err)
		}
	}

	if err := services.FillDecksFromInventories(config.DB); err != nil {
		log.Printf("Deck fill failed: %v", err)
	}

	r := chi.NewRouter()
	r.Use(cors.Handler(cors.Options{
		AllowedOrigins:   []string{"https://*"},
		AllowedMethods:   []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"Accept", "Authorization", "Content-Type", "X-CSRF-Token", "X-Session-ID"},
		ExposedHeaders:   []string{"Link"},
		AllowCredentials: true,
		MaxAge:           300,
	}))
	if httpRequestLoggingEnabled {
		r.Use(util.HTTPRequestLogger(debugLoggingEnabled))
	}

	// Serve card images for the client
	imageDir := resolveImageDir()
	log.Printf("Serving card images from %s", imageDir)
	r.Handle("/cards/assets/*", http.StripPrefix("/cards/assets", http.FileServer(http.Dir(imageDir))))

	r.Get("/cards/swagger/*", httpSwagger.Handler(
		httpSwagger.URL("/cards/swagger/doc.json"),
	))
	r.Get("/cards/health", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte("{\"status\":\"healthy\"}"))
	})

	r.Get("/cards/list", services.ListCards)
	r.Get("/cards/count", services.GetCardCount)
	r.Post("/cards/create", services.CreateCard)
	r.Put("/cards/update", services.UpdateCard)
	r.Delete("/cards/delete", services.DeleteCard)
	r.Get("/inventories/{uid}", services.GetInventoryByUserID)
	r.Post("/inventories", services.CreateInventory)
	r.Get("/inventories", services.ListInventories)
	r.Put("/inventories", services.UpdateInventory)
	r.Put("/inventories/coins", services.UpdateInventoryCoins)
	r.Put("/inventories/coins/add", services.AddInventoryCoins)
	r.Post("/decks", services.CreateDeck)
	r.Get("/decks", services.ListDecks)
	r.Get("/decks/{uid}", services.GetDeckByUserID)
	r.Delete("/decks", services.DeleteDeck)
	r.Post("/decks/fill", services.FillDeckForUser)
	r.Get("/payments/packages", services.ListCoinPackages)
	r.Post("/payments/checkout-session", services.CreateCoinCheckoutSession)
	r.Post("/payments/process-card", services.ProcessCardPayment)
	r.Post("/payments/webhook", services.HandlePaymentWebhook)
	r.Post("/payments/stripe-webhook", services.HandleStripeWebhook)
	r.Get("/payments/checkout-complete", services.RenderCheckoutCompletePage)
	r.Get("/payments/checkout-session-status", services.GetCheckoutSessionStatus)

	if err := http.ListenAndServeTLS(":"+portString, "certs/server.crt", "certs/server.key", r); err != nil {
		log.Fatal(err)
	}
}
