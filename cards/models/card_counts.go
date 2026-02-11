package models

import (
	"database/sql/driver"
	"encoding/json"
	"fmt"
)

// CardCounts stores card quantities keyed by card ID.
type CardCounts map[int]int

// Value marshals CardCounts to JSON for database storage.
func (c CardCounts) Value() (driver.Value, error) {
	if c == nil {
		return []byte("null"), nil
	}

	data, err := json.Marshal(c)
	if err != nil {
		return nil, err
	}

	return data, nil
}

// Scan unmarshals JSON database values into CardCounts.
func (c *CardCounts) Scan(value interface{}) error {
	if value == nil {
		*c = nil
		return nil
	}

	switch v := value.(type) {
	case []byte:
		return json.Unmarshal(v, c)
	case string:
		return json.Unmarshal([]byte(v), c)
	default:
		return fmt.Errorf("unsupported type: %T", value)
	}
}
