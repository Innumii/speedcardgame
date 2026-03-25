package util

import (
	"encoding/json"
	"net/http"
)

// DecodeJSONBody decodes request JSON payload and emits a consistent 400 response on failure.
func DecodeJSONBody(w http.ResponseWriter, r *http.Request, dst interface{}, invalidMessage string) bool {
	if err := json.NewDecoder(r.Body).Decode(dst); err != nil {
		RespondWithError(w, http.StatusBadRequest, invalidMessage)
		return false
	}
	return true
}
