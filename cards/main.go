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

// resolveImageDir finds the directory containing card images by checking
// a list of common relative paths. Returns the first valid directory found,
// or falls back to "assets/cards" if none exist.
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
	// --- Environment Setup ---

	// Load environment variables from .env file (non-fatal if missing).
	if err := godotenv.Load(".env"); err != nil {
		log.Printf(".env not loaded: %v", err)
	}

	// Read logging flags from environment.
	debugLoggingEnabled := util.GetEnvAsBool("DEBUG_LOG_ENABLED", false)
	httpRequestLoggingEnabled := util.GetEnvAsBool("HTTP_REQUEST_LOG_ENABLED", true)
	util.LogStartupConfiguration("cards", debugLoggingEnabled, httpRequestLoggingEnabled)

	// --- Server Port ---

	portString := os.Getenv("PORT")
	if portString == "" {
		log.Fatal("PORT not found in env")
	}
	fmt.Println("Port:", portString)

	// --- Database ---

	// Connect to PostgreSQL via GORM using the DATABASE_URL env variable.
	dsn := os.Getenv("DATABASE_URL")
	if err := config.InitializeDatabase(dsn); err != nil {
		log.Fatal("Failed to connect to database:", err)
	}

	// Auto-migrate all models to keep the schema in sync.
	if err := config.DB.AutoMigrate(
		&models.Deck{},
		&models.Card{},
		&models.Inventory{},
		&models.PaymentLedger{},
	); err != nil {
		log.Fatalf("Error auto-migrating tables: %v", err)
	}

	// --- Payments ---

	// Initialize the payments module. Logs a warning and continues if unavailable.
	if err := services.InitializePaymentsFromEnv(); err != nil {
		log.Printf("Payments module disabled: %v", err)
	} else {
		log.Printf("Payments module initialized")
	}

	// --- Seeding ---

	// Seed card data from a CSV file (defaults to "cards.csv").
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

	// Populate decks for any inventories that don't have one yet.
	if err := services.FillDecksFromInventories(config.DB); err != nil {
		log.Printf("Deck fill failed: %v", err)
	}

	// --- Router Setup ---

	r := chi.NewRouter()

	// CORS — restrict to HTTPS origins only.
	r.Use(cors.Handler(cors.Options{
		AllowedOrigins:   []string{"https://*"},
		AllowedMethods:   []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"Accept", "Authorization", "Content-Type", "X-CSRF-Token", "X-Session-ID"},
		ExposedHeaders:   []string{"Link"},
		AllowCredentials: true,
		MaxAge:           300,
	}))

	// Optionally attach HTTP request logging middleware.
	if httpRequestLoggingEnabled {
		r.Use(util.HTTPRequestLogger(debugLoggingEnabled))
	}

	// --- Routes ---

	imageDir := resolveImageDir()
	log.Printf("Serving card images from %s", imageDir)

	r.Route("/cards", func(r chi.Router) {

		// Swagger UI
		r.Get("/swagger/*", httpSwagger.Handler(
			httpSwagger.URL("/cards/swagger/doc.json"),
		))

		// Health check
		r.Get("/health", func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "application/json")
			w.WriteHeader(http.StatusOK)
			_, _ = w.Write([]byte(`{"status":"healthy"}`))
		})

		// Static assets — primary and legacy endpoints
		r.Handle("/images/*", http.StripPrefix("/cards/images", http.FileServer(http.Dir(imageDir))))

		// Cards
		r.Get("/cards", services.ListCards)
		r.Get("/list", services.ListCards)     // Alias for /cards/cards
		r.Get("/count", services.GetCardCount)
		r.Post("/create", services.CreateCard)
		r.Put("/update", services.UpdateCard)
		r.Delete("/delete", services.DeleteCard)

		// Inventories
		r.Get("/inventories/{uid}", services.GetInventoryByUserID)
		r.Get("/inventories", services.ListInventories)
		r.Post("/inventories", services.CreateInventory)
		r.Put("/inventories", services.UpdateInventory)
		r.Put("/inventories/coins", services.UpdateInventoryCoins)
		r.Put("/inventories/coins/add", services.AddInventoryCoins)

		// Decks
		r.Get("/decks", services.ListDecks)
		r.Get("/decks/{uid}", services.GetDeckByUserID)
		r.Post("/decks", services.CreateDeck)
		r.Post("/decks/fill", services.FillDeckForUser)
		r.Delete("/decks", services.DeleteDeck)

		// Payments
		r.Route("/payments", func(r chi.Router) {
			r.Get("/coin-packages", services.ListCoinPackages)
			r.Post("/checkout-session", services.CreateCoinCheckoutSession)
			r.Post("/process-card", services.ProcessCardPayment)
			r.Post("/webhook", services.HandlePaymentWebhook)
			r.Post("/stripe-webhook", services.HandleStripeWebhook)
			r.Get("/checkout-complete", services.RenderCheckoutCompletePage)
			r.Get("/checkout-status", services.GetCheckoutSessionStatus)
			r.Get("/checkout-session-status", services.GetCheckoutSessionStatus) // Legacy alias
		})
	})

	// --- Start HTTPS Server ---

	if err := http.ListenAndServeTLS(":"+portString, "certs/server.crt", "certs/server.key", r); err != nil {
		log.Fatal(err)
	}
}