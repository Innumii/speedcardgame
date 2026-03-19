package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/Ryanljk/speedcardgame/cards/services"
	"github.com/Ryanljk/speedcardgame/cards/util"
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
	if err := config.DB.AutoMigrate(&models.Deck{}, &models.Card{}, &models.Inventory{}); err != nil {
		log.Fatalf("Error auto-migrating tables: %v", err)
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
	} else {
		log.Printf("Card seed loaded from %s", seedPath)
	}

	if err := services.FillDecksFromInventories(config.DB); err != nil {
		log.Printf("Deck fill failed: %v", err)
	}

	fmt.Println("Connecting to database:", dsn)
	//db.Exec("CREATE TABLE test_table (id SERIAL PRIMARY KEY, name VARCHAR(100));")  //create table just for testing purposes

	//Define the router
	router := chi.NewRouter() //setup router
	if httpRequestLoggingEnabled {
		router.Use(util.HTTPRequestLogger(debugLoggingEnabled))
	}
	router.Use(cors.Handler(cors.Options{
		AllowedOrigins:   []string{"https://*", "http://*"},
		AllowedMethods:   []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"*"},
		ExposedHeaders:   []string{"Link"},
		AllowCredentials: false,
		MaxAge:           300,
	}))

	r := chi.NewRouter()
	//define routes
	r.Get("/health", health) // Check health of service
	imageDir := resolveImageDir()
	log.Printf("Serving card images from %s", imageDir)
	r.Get("/images/{filename}", func(w http.ResponseWriter, req *http.Request) {
		filename := chi.URLParam(req, "filename")
		if filename == "" {
			http.NotFound(w, req)
			return
		}

		cleaned := filepath.Clean(filename)
		if cleaned == "." || strings.Contains(cleaned, "..") || cleaned != filepath.Base(cleaned) {
			http.NotFound(w, req)
			return
		}

		http.ServeFile(w, req, filepath.Join(imageDir, cleaned))
	})

	r.Get("/decks/{uid}", services.GetDeckByUserID) // Get deck by user ID
	r.Post("/decks", services.CreateDeck)           // Create deck
	r.Post("/decks/fill", services.FillDeckForUser)
	r.Get("/decks", services.ListDecks) // List all decks
	r.Delete("/decks", services.DeleteDeck)

	r.Post("/cards", services.CreateCard) // Create card
	r.Get("/cards", services.ListCards)   // List all cards
	r.Put("/cards", services.UpdateCard)
	r.Delete("/cards", services.DeleteCard)

	r.Post("/inventories", services.CreateInventory)           // Create inventory
	r.Get("/inventories", services.ListInventories)            // List all inventories
	r.Get("/inventories/{uid}", services.GetInventoryByUserID) // Get inventory by user ID
	r.Put("/inventories", services.UpdateInventory)            // Update inventory by user ID
	r.Put("/inventories/coins", services.UpdateInventoryCoins)    // Update inventory coins by user ID
	r.Put("/inventories/coins/add", services.AddInventoryCoins)    // Add coins/subtract

	// router.Mount("/cardbase", r) //api prefix
	router.Mount("/cards", r)

	srv := &http.Server{
		Handler: router,
		Addr:    ":" + portString,
	}

	//start server on port
	if err := srv.ListenAndServe(); err != nil {
		log.Fatal(err)
	}

}

func health(w http.ResponseWriter, r *http.Request) {
	response := map[string]string{"message": "Healthy"}
	util.RespondWithJSON(w, 200, response)
}
