package services

import (
	"encoding/csv"
	"errors"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"

	"github.com/FYL-Studios/speedcardgame/cards/models"
	"gorm.io/gorm"
)

var cardCSVColumns = []string{"cid", "name", "type", "cost", "value", "power", "toughness", "effect"}

// SeedCardsFromCSV loads cards from a CSV file and upserts by cid.
func SeedCardsFromCSV(db *gorm.DB, path string) error {
	file, err := os.Open(path)
	if err != nil {
		return err
	}
	defer file.Close()

	reader := csv.NewReader(file)
	reader.FieldsPerRecord = -1

	header, err := reader.Read()
	if err != nil {
		return fmt.Errorf("read header: %w", err)
	}

	columnIndex := map[string]int{}
	for i, name := range header {
		columnIndex[strings.ToLower(strings.TrimSpace(name))] = i
	}

	for _, col := range cardCSVColumns {
		if _, ok := columnIndex[col]; !ok {
			return fmt.Errorf("missing required column: %s", col)
		}
	}

	line := 1
	for {
		record, err := reader.Read()
		if errors.Is(err, io.EOF) {
			break
		}
		if err != nil {
			return fmt.Errorf("read line %d: %w", line+1, err)
		}
		line++

		if len(record) == 0 {
			continue
		}

		cid, err := parseCSVInt(record, columnIndex["cid"], "cid", line)
		if err != nil {
			return err
		}

		name := strings.TrimSpace(GetCSVField(record, columnIndex["name"]))
		cardType := strings.TrimSpace(GetCSVField(record, columnIndex["type"]))
		if name == "" || cardType == "" {
			return fmt.Errorf("line %d: name and type are required", line)
		}

		cost, err := parseCSVInt(record, columnIndex["cost"], "cost", line)
		if err != nil {
			return err
		}
		value, err := parseCSVInt(record, columnIndex["value"], "value", line)
		if err != nil {
			return err
		}
		power, err := parseCSVInt(record, columnIndex["power"], "power", line)
		if err != nil {
			return err
		}
		toughness, err := parseCSVInt(record, columnIndex["toughness"], "toughness", line)
		if err != nil {
			return err
		}
		effect := strings.TrimSpace(GetCSVField(record, columnIndex["effect"]))

		card := models.Card{
			Cid:       cid,
			Name:      name,
			Type:      cardType,
			Cost:      cost,
			Value:     value,
			Power:     power,
			Toughness: toughness,
			Effect:    effect,
		}

		var existing models.Card
		lookup := db.Where("cid = ?", card.Cid).First(&existing)
		if errors.Is(lookup.Error, gorm.ErrRecordNotFound) {
			if err := db.Create(&card).Error; err != nil {
				return fmt.Errorf("insert cid %d: %w", card.Cid, err)
			}
			continue
		}
		if lookup.Error != nil {
			return fmt.Errorf("lookup cid %d: %w", card.Cid, lookup.Error)
		}

		updates := map[string]interface{}{
			"name":      card.Name,
			"type":      card.Type,
			"cost":      card.Cost,
			"value":     card.Value,
			"power":     card.Power,
			"toughness": card.Toughness,
			"effect":    card.Effect,
		}
		if err := db.Model(&existing).Updates(updates).Error; err != nil {
			return fmt.Errorf("update cid %d: %w", card.Cid, err)
		}
	}

	return nil
}

func parseCSVInt(record []string, index int, field string, line int) (int, error) {
	raw := strings.TrimSpace(GetCSVField(record, index))
	if raw == "" {
		return 0, nil
	}
	value, err := strconv.Atoi(raw)
	if err != nil {
		return 0, fmt.Errorf("line %d: invalid %s: %w", line, field, err)
	}
	return value, nil
}

// GetCSVField returns the value at index, or an empty string when out of bounds.
func GetCSVField(record []string, index int) string {
	if index < 0 || index >= len(record) {
		return ""
	}
	return record[index]
}
