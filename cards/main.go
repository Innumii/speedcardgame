package main

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"strings"

	_ "github.com/FYL-Studios/speedcardgame/cards/docs"
	"github.com/FYL-Studios/speedcardgame/cards/middleware"
	"github.com/redis/go-redis/v9"
	httpSwagger "github.com/swaggo/http-swagger"

	"github.com/FYL-Studios/speedcardgame/cards/config"
	"github.com/FYL-Studios/speedcardgame/cards/models"
	"github.com/FYL-Studios/speedcardgame/cards/services"
	"github.com/FYL-Studios/speedcardgame/cards/util"
	"github.com/go-chi/chi"
	"github.com/go-chi/cors"
	"github.com/joho/godotenv"
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

func serveCardImage(imageDir string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		cardID := chi.URLParam(r, "id")
		if cardID == "" {
			cardID = chi.URLParam(r, "*")
		}
		if cardID == "" {
			cardID = path.Base(r.URL.Path)
		}
		cardID = strings.TrimSuffix(cardID, ".png")
		if cardID == "" || cardID == "." || cardID == "/" {
			http.NotFound(w, r)
			return
		}

		http.ServeFile(w, r, filepath.Join(imageDir, cardID+".png"))
	}
}

func getEnvOrDefault(key, fallback string) string {
	value := os.Getenv(key)
	if value == "" {
		return fallback
	}
	return value
}

func buildPostgresDSN() string {
	host := getEnvOrDefault("POSTGRES_HOST", "localhost")
	user := getEnvOrDefault("POSTGRES_USER", "postgres")
	password := getEnvOrDefault("POSTGRES_PASSWORD", "postgres")
	database := getEnvOrDefault("POSTGRES_DB", "cardsdb")
	port := getEnvOrDefault("POSTGRES_PORT", "5432")
	sslMode := getEnvOrDefault("POSTGRES_SSLMODE", "disable")
	timezone := getEnvOrDefault("POSTGRES_TIMEZONE", "UTC")

	return fmt.Sprintf(
		"host=%s user=%s password=%s dbname=%s port=%s sslmode=%s TimeZone=%s",
		host,
		user,
		password,
		database,
		port,
		sslMode,
		timezone,
	)
}

func main() {
	// --- Environment Setup ---

	// Load environment variables from .env file (non-fatal if missing).
	loadOptionalEnvFiles("../.env", "./.env")
	// Read logging flags from environment.
	debugLoggingEnabled := util.GetEnvAsBool("DEBUG_LOG_ENABLED", false)
	httpRequestLoggingEnabled := util.GetEnvAsBool("HTTP_REQUEST_LOG_ENABLED", true)
	util.LogStartupConfiguration("cards", debugLoggingEnabled, httpRequestLoggingEnabled)

	// --- Server Port ---

	portString := os.Getenv("DOCKER_CARDS_SERVICE_PORT")
	if portString == "" {
		portString = os.Getenv("CARDS_SERVICE_PORT")
	}
	if portString == "" {
		portString = "8080"
	}
	fmt.Println("Port:", portString)

	// --- Database ---

	// Build DSN from auth-style POSTGRES_* environment variables.
	dsn := buildPostgresDSN()
	if err := config.InitializeDatabase(dsn); err != nil {
		log.Fatal("Failed to connect to database:", err)
	}

	redisAddr := fmt.Sprintf("%s:%s", os.Getenv("REDIS_HOST"), os.Getenv("REDIS_PORT"))
	redisClient := redis.NewClient(&redis.Options{
		Addr: redisAddr, // Redis server address
	})

	ctx := context.Background()
	_, err := redisClient.Ping(ctx).Result()
	if err != nil {
		log.Fatal("Failed to connect to Redis:", err)
	}
	fmt.Println("Connected to Redis successfully")

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
		AllowedHeaders:   []string{"Accept", "Authorization", "Content-Type", "X-CSRF-Token", "X-Session-ID", "X-Internal-Key"},
		ExposedHeaders:   []string{"Link"},
		AllowCredentials: true,
		MaxAge:           300,
	}))
	r.Use(func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			fmt.Printf("[cards] incoming: %s %s\n", r.Method, r.URL.Path)
			next.ServeHTTP(w, r)
		})
	})

	// Optionally attach HTTP request logging middleware.
	if httpRequestLoggingEnabled {
		r.Use(util.HTTPRequestLogger(debugLoggingEnabled))
	}

	// --- Routes ---

	imageDir := resolveImageDir()
	log.Printf("Serving card images from %s", imageDir)

	r.Route("/cards", func(r chi.Router) {
		// ── Public routes ────────────────────────────────

		r.Get("/health", func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "application/json")
			w.WriteHeader(http.StatusOK)
			_, _ = w.Write([]byte(`{"status":"healthy"}`))
		})
		r.Get("/swagger/*", httpSwagger.Handler(
			httpSwagger.URL("/cards/swagger/doc.json"),
		))

		r.Route("/payments", func(r chi.Router) {
			r.Get("/checkout-status", services.GetCheckoutSessionStatus)
			r.Get("/checkout-complete", services.RenderCheckoutCompletePage)
			r.Post("/webhook", services.HandlePaymentWebhook)
			r.Post("/stripe-webhook", services.HandleStripeWebhook)

			r.Group(func(r chi.Router) {
				r.Use(middleware.RequireAuth(redisClient))

				r.Get("/coin-packages", services.ListCoinPackages)
				r.Post("/checkout-session", services.CreateCoinCheckoutSession)
				r.Post("/process-card", services.ProcessCardPayment)
			})
		})
		// ── Protected routes ─────────────────────────────
		r.Group(func(r chi.Router) {
			r.Use(middleware.RequireAuth(redisClient))

			// Cards
			r.Get("/cards", services.ListCards)
			r.Get("/images/{id}", serveCardImage(imageDir))
			r.Get("/list", services.ListCards)
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

		})
	})

	// --- Start HTTPS Server ---

	if err := http.ListenAndServeTLS(":"+portString, "certs/server.crt", "certs/server.key", r); err != nil {
		log.Fatal(err)
	}
}
