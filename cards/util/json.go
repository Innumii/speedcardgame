package util

import ("net/http"
		"encoding/json"
		"log")

func RespondWithJSON(w http.ResponseWriter, code int, payload interface{}) {
	data, err := json.Marshal(payload) //marshal payload data into a json format
	if err != nil {
		log.Println("Failed to marshal JSON response: %v", payload)
		w.WriteHeader(500)
		return
	}
	w.Header().Add("Content-Type", "application/json") //response header
	w.WriteHeader(code) //OK
	w.Write(data)
}