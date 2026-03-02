package util

import (
	"encoding/json"
	"log"
	"net/http"
)

func RespondWithJSON(w http.ResponseWriter, code int, payload interface{}) {
	data, err := json.Marshal(payload) //marshal payload data into a json format
	if err != nil {
		log.Printf("Failed to marshal JSON response: %+v", payload)
		w.WriteHeader(500)
		return
	}
	w.Header().Add("Content-Type", "application/json") //response header
	w.WriteHeader(code)                                //OK
	if _, err := w.Write(data); err != nil {
		log.Printf("Failed to write JSON response: %v", err)
	}
}

func RespondWithError(w http.ResponseWriter, code int, message string) {
	RespondWithJSON(w, code, map[string]string{"error": message})
}