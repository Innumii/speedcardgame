package services_test

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/Ryanljk/speedcardgame/cards/payments"
	cardservices "github.com/Ryanljk/speedcardgame/cards/services"
)

type fakePaymentClient struct {
	createResponse payments.CheckoutSessionResponse
	createErr      error
	webhookEvent   payments.WebhookEvent
	webhookErr     error
}

func (f *fakePaymentClient) CreateCheckoutSession(_ context.Context, _ payments.CheckoutSessionRequest) (payments.CheckoutSessionResponse, error) {
	if f.createErr != nil {
		return payments.CheckoutSessionResponse{}, f.createErr
	}
	return f.createResponse, nil
}

func (f *fakePaymentClient) ProcessCardPayment(_ context.Context, _ payments.DirectCardPaymentRequest) (payments.DirectCardPaymentResponse, error) {
	if f.createErr != nil {
		return payments.DirectCardPaymentResponse{}, f.createErr
	}
	return payments.DirectCardPaymentResponse{ID: "ch_test", Status: "succeeded", Paid: true}, nil
}

func (f *fakePaymentClient) ParseWebhook(_ []byte, _ string) (payments.WebhookEvent, error) {
	if f.webhookErr != nil {
		return payments.WebhookEvent{}, f.webhookErr
	}
	return f.webhookEvent, nil
}

func (f *fakePaymentClient) GetCheckoutSession(_ context.Context, sessionID string) (payments.CheckoutSessionPaid, error) {
	if f.webhookErr != nil {
		return payments.CheckoutSessionPaid{}, f.webhookErr
	}
	return payments.CheckoutSessionPaid{
		ID:           sessionID,
		PaymentState: "paid",
		Metadata: map[string]string{
			"uid":        "7",
			"coins":      "1000",
			"package_id": "coin_pack_small",
		},
	}, nil
}

func TestListCoinPackages(t *testing.T) {
	cardservices.SetPaymentClientForTests(&fakePaymentClient{})
	t.Cleanup(func() { cardservices.SetPaymentClientForTests(nil) })

	rr := httptest.NewRecorder()
	cardservices.ListCoinPackages(rr, httptest.NewRequest(http.MethodGet, "/payments/packages", nil))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}

	var payload map[string][]payments.CoinPackage
	if err := json.NewDecoder(rr.Body).Decode(&payload); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}

	if len(payload["packages"]) == 0 {
		t.Fatal("expected at least one coin package")
	}
}

func TestCreateCoinCheckoutSession_Success(t *testing.T) {
	client := &fakePaymentClient{createResponse: payments.CheckoutSessionResponse{ID: "cs_123", URL: "https://checkout.stripe.com/pay/cs_123"}}
	cardservices.SetPaymentClientForTests(client)
	t.Cleanup(func() { cardservices.SetPaymentClientForTests(nil) })

	body := `{"uid": 12, "package_id": "coin_pack_small", "success_url":"https://game.example/success", "cancel_url":"https://game.example/cancel"}`
	rr := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodPost, "/payments/checkout-session", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")

	cardservices.CreateCoinCheckoutSession(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
}

func TestCreateCoinCheckoutSession_UnsupportedPackage(t *testing.T) {
	cardservices.SetPaymentClientForTests(&fakePaymentClient{})
	t.Cleanup(func() { cardservices.SetPaymentClientForTests(nil) })

	body := `{"uid": 12, "package_id": "not_allowed", "success_url":"https://game.example/success", "cancel_url":"https://game.example/cancel"}`
	rr := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodPost, "/payments/checkout-session", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")

	cardservices.CreateCoinCheckoutSession(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d", rr.Code)
	}
}

func TestCreateCoinCheckoutSession_ProviderError(t *testing.T) {
	client := &fakePaymentClient{createErr: errors.New("stripe unavailable")}
	cardservices.SetPaymentClientForTests(client)
	t.Cleanup(func() { cardservices.SetPaymentClientForTests(nil) })

	body := `{"uid": 12, "package_id": "coin_pack_small", "success_url":"https://game.example/success", "cancel_url":"https://game.example/cancel"}`
	rr := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodPost, "/payments/checkout-session", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")

	cardservices.CreateCoinCheckoutSession(rr, req)

	if rr.Code != http.StatusBadGateway {
		t.Fatalf("expected 502, got %d", rr.Code)
	}
}

func TestHandleStripeWebhook_AppliesCoinsIdempotently(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 7, Coins: 100, Cards: models.CardCounts{1: 1}})

	client := &fakePaymentClient{
		webhookEvent: payments.WebhookEvent{
			ID:   "evt_123",
			Type: "checkout.session.completed",
			Session: &payments.CheckoutSessionPaid{
				ID:           "cs_test",
				PaymentState: "paid",
				Metadata: map[string]string{
					"uid":        "7",
					"coins":      "1000",
					"package_id": "coin_pack_small",
				},
			},
		},
	}
	cardservices.SetPaymentClientForTests(client)
	t.Cleanup(func() { cardservices.SetPaymentClientForTests(nil) })

	for i := 0; i < 2; i++ {
		rr := httptest.NewRecorder()
		req := httptest.NewRequest(http.MethodPost, "/payments/webhook", strings.NewReader("{}"))
		req.Header.Set("Stripe-Signature", "sig")
		cardservices.HandleStripeWebhook(rr, req)
		if rr.Code != http.StatusOK {
			t.Fatalf("expected 200, got %d", rr.Code)
		}
	}

	var inv models.Inventory
	if err := db.Where("uid = ?", 7).First(&inv).Error; err != nil {
		t.Fatalf("failed to fetch inventory: %v", err)
	}

	if inv.Coins != 1100 {
		t.Fatalf("expected 1100 coins after idempotent webhook handling, got %d", inv.Coins)
	}
}
