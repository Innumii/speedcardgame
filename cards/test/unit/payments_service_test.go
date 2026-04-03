package services_test

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/Ryanljk/speedcardgame/cards/payments"
	cardservices "github.com/Ryanljk/speedcardgame/cards/services"
)

// ── Mock payment client ────────────────────────────────────────────────────────

type mockPaymentClient struct {
	createCheckoutSessionFn func(ctx context.Context, req payments.CheckoutSessionRequest) (payments.CheckoutSessionResponse, error)
	processCardPaymentFn    func(ctx context.Context, req payments.DirectCardPaymentRequest) (payments.DirectCardPaymentResponse, error)
	parseWebhookFn          func(payload []byte, headers http.Header) (payments.WebhookEvent, error)
	getCheckoutSessionFn    func(ctx context.Context, sessionID string) (payments.CheckoutSessionPaid, error)
}

func (m *mockPaymentClient) CreateCheckoutSession(ctx context.Context, req payments.CheckoutSessionRequest) (payments.CheckoutSessionResponse, error) {
	return m.createCheckoutSessionFn(ctx, req)
}

func (m *mockPaymentClient) ProcessCardPayment(ctx context.Context, req payments.DirectCardPaymentRequest) (payments.DirectCardPaymentResponse, error) {
	return m.processCardPaymentFn(ctx, req)
}

func (m *mockPaymentClient) ParseWebhook(payload []byte, headers http.Header) (payments.WebhookEvent, error) {
	return m.parseWebhookFn(payload, headers)
}

func (m *mockPaymentClient) GetCheckoutSession(ctx context.Context, sessionID string) (payments.CheckoutSessionPaid, error) {
	return m.getCheckoutSessionFn(ctx, sessionID)
}

func setupMockPaymentClient(t *testing.T, client payments.Client) {
	t.Helper()
	cardservices.SetPaymentClientForTests(client)
	t.Cleanup(func() { cardservices.SetPaymentClientForTests(nil) })
}

// ── GetCardCount ───────────────────────────────────────────────────────────────

func TestGetCardCount_Empty(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.GetCardCount(rr, httptest.NewRequest(http.MethodGet, "/cards/count", nil))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var result map[string]int64
	if err := json.NewDecoder(rr.Body).Decode(&result); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if result["count"] != 0 {
		t.Errorf("expected 0, got %d", result["count"])
	}
}

func TestGetCardCount_WithCards(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "A"}, {Cid: 2, Name: "B"}, {Cid: 3, Name: "C"}})
	rr := httptest.NewRecorder()
	cardservices.GetCardCount(rr, httptest.NewRequest(http.MethodGet, "/cards/count", nil))
	var result map[string]int64
	if err := json.NewDecoder(rr.Body).Decode(&result); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if result["count"] != 3 {
		t.Errorf("expected 3, got %d", result["count"])
	}
}

func TestGetCardCount_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.GetCardCount(rr, httptest.NewRequest(http.MethodGet, "/cards/count", nil))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

// ── ListCoinPackages ───────────────────────────────────────────────────────────

func TestListCoinPackages_Success(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.ListCoinPackages(rr, httptest.NewRequest(http.MethodGet, "/payments/packages", nil))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var result map[string]interface{}
	if err := json.NewDecoder(rr.Body).Decode(&result); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if _, ok := result["packages"]; !ok {
		t.Error("expected 'packages' key in response")
	}
}

// ── CreateCoinCheckoutSession ──────────────────────────────────────────────────

func TestCreateCoinCheckoutSession_NilClient(t *testing.T) {
	setupTestDB(t)
	cardservices.SetPaymentClientForTests(nil)
	body := map[string]interface{}{"uid": 1, "package_id": "coin_pack_small", "success_url": "https://success", "cancel_url": "https://cancel"}
	rr := httptest.NewRecorder()
	cardservices.CreateCoinCheckoutSession(rr, jsonRequest(t, http.MethodPost, "/payments/checkout", body))
	if rr.Code != http.StatusServiceUnavailable {
		t.Errorf("expected 503, got %d", rr.Code)
	}
}

func TestCreateCoinCheckoutSession_InvalidBody(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{})
	rr := httptest.NewRecorder()
	cardservices.CreateCoinCheckoutSession(rr, httptest.NewRequest(http.MethodPost, "/payments/checkout", bytes.NewBufferString("bad")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateCoinCheckoutSession_InvalidUID(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{})
	body := map[string]interface{}{"uid": 0, "package_id": "coin_pack_small", "success_url": "https://success", "cancel_url": "https://cancel"}
	rr := httptest.NewRecorder()
	cardservices.CreateCoinCheckoutSession(rr, jsonRequest(t, http.MethodPost, "/payments/checkout", body))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateCoinCheckoutSession_InvalidPackage(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{})
	body := map[string]interface{}{"uid": 1, "package_id": "invalid_pack", "success_url": "https://success", "cancel_url": "https://cancel"}
	rr := httptest.NewRecorder()
	cardservices.CreateCoinCheckoutSession(rr, jsonRequest(t, http.MethodPost, "/payments/checkout", body))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateCoinCheckoutSession_ClientError(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		createCheckoutSessionFn: func(_ context.Context, _ payments.CheckoutSessionRequest) (payments.CheckoutSessionResponse, error) {
			return payments.CheckoutSessionResponse{}, errors.New("stripe error")
		},
	})
	body := map[string]interface{}{"uid": 1, "package_id": "coin_pack_small", "success_url": "https://success", "cancel_url": "https://cancel"}
	rr := httptest.NewRecorder()
	cardservices.CreateCoinCheckoutSession(rr, jsonRequest(t, http.MethodPost, "/payments/checkout", body))
	if rr.Code != http.StatusBadGateway {
		t.Errorf("expected 502, got %d", rr.Code)
	}
}

func TestCreateCoinCheckoutSession_Success(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		createCheckoutSessionFn: func(_ context.Context, _ payments.CheckoutSessionRequest) (payments.CheckoutSessionResponse, error) {
			return payments.CheckoutSessionResponse{ID: "cs_123", URL: "https://stripe.com/pay"}, nil
		},
	})
	body := map[string]interface{}{"uid": 1, "package_id": "coin_pack_small", "success_url": "https://success", "cancel_url": "https://cancel"}
	rr := httptest.NewRecorder()
	cardservices.CreateCoinCheckoutSession(rr, jsonRequest(t, http.MethodPost, "/payments/checkout", body))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
}

// ── RenderCheckoutCompletePage ─────────────────────────────────────────────────

func TestRenderCheckoutCompletePage_Success(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/payments/checkout-complete?status=success", nil)
	rr := httptest.NewRecorder()
	cardservices.RenderCheckoutCompletePage(rr, req)
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	if rr.Header().Get("Content-Type") != "text/html; charset=utf-8" {
		t.Errorf("expected HTML content type")
	}
	body := rr.Body.String()
	if !bytes.Contains([]byte(body), []byte("Payment successful")) {
		t.Error("expected 'Payment successful' in response body")
	}
}

func TestRenderCheckoutCompletePage_Cancel(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/payments/checkout-complete?status=cancel", nil)
	rr := httptest.NewRecorder()
	cardservices.RenderCheckoutCompletePage(rr, req)
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	body := rr.Body.String()
	if !bytes.Contains([]byte(body), []byte("Payment canceled")) {
		t.Error("expected 'Payment canceled' in response body")
	}
}

func TestRenderCheckoutCompletePage_UnknownStatus(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/payments/checkout-complete?status=unknown", nil)
	rr := httptest.NewRecorder()
	cardservices.RenderCheckoutCompletePage(rr, req)
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	body := rr.Body.String()
	if !bytes.Contains([]byte(body), []byte("Payment flow completed")) {
		t.Error("expected 'Payment flow completed' in response body")
	}
}

// ── ProcessCardPayment ─────────────────────────────────────────────────────────

func TestProcessCardPayment_Disabled(t *testing.T) {
	t.Setenv("ALLOW_DIRECT_CARD_PAYMENTS", "false")
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.ProcessCardPayment(rr, httptest.NewRequest(http.MethodPost, "/payments/card", nil))
	if rr.Code != http.StatusForbidden {
		t.Errorf("expected 403, got %d", rr.Code)
	}
}

func TestProcessCardPayment_NilClient(t *testing.T) {
	t.Setenv("ALLOW_DIRECT_CARD_PAYMENTS", "true")
	setupTestDB(t)
	cardservices.SetPaymentClientForTests(nil)
	rr := httptest.NewRecorder()
	cardservices.ProcessCardPayment(rr, httptest.NewRequest(http.MethodPost, "/payments/card", nil))
	if rr.Code != http.StatusServiceUnavailable {
		t.Errorf("expected 503, got %d", rr.Code)
	}
}

func TestProcessCardPayment_InvalidBody(t *testing.T) {
	t.Setenv("ALLOW_DIRECT_CARD_PAYMENTS", "true")
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{})
	rr := httptest.NewRecorder()
	cardservices.ProcessCardPayment(rr, httptest.NewRequest(http.MethodPost, "/payments/card", bytes.NewBufferString("bad")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestProcessCardPayment_InvalidRequest(t *testing.T) {
	t.Setenv("ALLOW_DIRECT_CARD_PAYMENTS", "true")
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{})
	body := map[string]interface{}{"uid": 0}
	rr := httptest.NewRecorder()
	cardservices.ProcessCardPayment(rr, jsonRequest(t, http.MethodPost, "/payments/card", body))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestProcessCardPayment_InvalidPackage(t *testing.T) {
	t.Setenv("ALLOW_DIRECT_CARD_PAYMENTS", "true")
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{})
	body := map[string]interface{}{"uid": 1, "package_id": "invalid_pack", "card_number": "4242424242424242", "cvc": "123", "cardholder_name": "Test"}
	rr := httptest.NewRecorder()
	cardservices.ProcessCardPayment(rr, jsonRequest(t, http.MethodPost, "/payments/card", body))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestProcessCardPayment_ClientError(t *testing.T) {
	t.Setenv("ALLOW_DIRECT_CARD_PAYMENTS", "true")
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		processCardPaymentFn: func(_ context.Context, _ payments.DirectCardPaymentRequest) (payments.DirectCardPaymentResponse, error) {
			return payments.DirectCardPaymentResponse{}, errors.New("stripe error")
		},
	})
	body := map[string]interface{}{"uid": 1, "package_id": "coin_pack_small", "card_number": "4242424242424242", "cvc": "123", "cardholder_name": "Test"}
	rr := httptest.NewRecorder()
	cardservices.ProcessCardPayment(rr, jsonRequest(t, http.MethodPost, "/payments/card", body))
	if rr.Code != http.StatusBadGateway {
		t.Errorf("expected 502, got %d", rr.Code)
	}
}

func TestProcessCardPayment_NotPaid(t *testing.T) {
	t.Setenv("ALLOW_DIRECT_CARD_PAYMENTS", "true")
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		processCardPaymentFn: func(_ context.Context, _ payments.DirectCardPaymentRequest) (payments.DirectCardPaymentResponse, error) {
			return payments.DirectCardPaymentResponse{ID: "ch_123", Paid: false}, nil
		},
	})
	body := map[string]interface{}{"uid": 1, "package_id": "coin_pack_small", "card_number": "4242424242424242", "cvc": "123", "cardholder_name": "Test"}
	rr := httptest.NewRecorder()
	cardservices.ProcessCardPayment(rr, jsonRequest(t, http.MethodPost, "/payments/card", body))
	if rr.Code != http.StatusPaymentRequired {
		t.Errorf("expected 402, got %d", rr.Code)
	}
}

// ── HandlePaymentWebhook ───────────────────────────────────────────────────────

func TestHandlePaymentWebhook_NilClient(t *testing.T) {
	setupTestDB(t)
	cardservices.SetPaymentClientForTests(nil)
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", nil))
	if rr.Code != http.StatusServiceUnavailable {
		t.Errorf("expected 503, got %d", rr.Code)
	}
}

func TestHandlePaymentWebhook_ParseError(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{}, errors.New("invalid signature")
		},
	})
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestHandlePaymentWebhook_NotCheckoutCompleted(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{ID: "evt_123", CheckoutCompleted: false}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr.Code != http.StatusOK {
		t.Errorf("expected 200, got %d", rr.Code)
	}
}

func TestHandlePaymentWebhook_NilSession(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{ID: "evt_123", CheckoutCompleted: true, Session: nil}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestHandlePaymentWebhook_NotPaid(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{
				ID:                "evt_123",
				CheckoutCompleted: true,
				Session:           &payments.CheckoutSessionPaid{ID: "cs_123", Paid: false},
			}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr.Code != http.StatusOK {
		t.Errorf("expected 200, got %d", rr.Code)
	}
}

func TestHandlePaymentWebhook_MissingMetadata(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{
				ID:                "evt_123",
				CheckoutCompleted: true,
				Session: &payments.CheckoutSessionPaid{
					ID:       "cs_123",
					Paid:     true,
					Metadata: map[string]string{},
				},
			}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestHandlePaymentWebhook_InvalidUID(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{
				ID:                "evt_123",
				CheckoutCompleted: true,
				Session: &payments.CheckoutSessionPaid{
					ID:   "cs_123",
					Paid: true,
					Metadata: map[string]string{
						"uid": "notanint", "package_id": "coin_pack_small", "coins": "1000",
					},
				},
			}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestHandlePaymentWebhook_InvalidCoins(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{
				ID:                "evt_123",
				CheckoutCompleted: true,
				Session: &payments.CheckoutSessionPaid{
					ID:   "cs_123",
					Paid: true,
					Metadata: map[string]string{
						"uid": "1", "package_id": "coin_pack_small", "coins": "notanint",
					},
				},
			}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestHandlePaymentWebhook_InvalidPackage(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{
				ID:                "evt_123",
				CheckoutCompleted: true,
				Session: &payments.CheckoutSessionPaid{
					ID:   "cs_123",
					Paid: true,
					Metadata: map[string]string{
						"uid": "1", "package_id": "invalid_pack", "coins": "1000",
					},
				},
			}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── GetCheckoutSessionStatus ───────────────────────────────────────────────────

func TestGetCheckoutSessionStatus_NilClient(t *testing.T) {
	setupTestDB(t)
	cardservices.SetPaymentClientForTests(nil)
	rr := httptest.NewRecorder()
	cardservices.GetCheckoutSessionStatus(rr, httptest.NewRequest(http.MethodGet, "/payments/session-status?session_id=cs_123", nil))
	if rr.Code != http.StatusServiceUnavailable {
		t.Errorf("expected 503, got %d", rr.Code)
	}
}

func TestGetCheckoutSessionStatus_MissingSessionID(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{})
	rr := httptest.NewRecorder()
	cardservices.GetCheckoutSessionStatus(rr, httptest.NewRequest(http.MethodGet, "/payments/session-status", nil))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestGetCheckoutSessionStatus_ClientError(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		getCheckoutSessionFn: func(_ context.Context, _ string) (payments.CheckoutSessionPaid, error) {
			return payments.CheckoutSessionPaid{}, errors.New("stripe error")
		},
	})
	rr := httptest.NewRecorder()
	cardservices.GetCheckoutSessionStatus(rr, httptest.NewRequest(http.MethodGet, "/payments/session-status?session_id=cs_123", nil))
	if rr.Code != http.StatusBadGateway {
		t.Errorf("expected 502, got %d", rr.Code)
	}
}

func TestGetCheckoutSessionStatus_NotPaid(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		getCheckoutSessionFn: func(_ context.Context, _ string) (payments.CheckoutSessionPaid, error) {
			return payments.CheckoutSessionPaid{ID: "cs_123", Paid: false}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.GetCheckoutSessionStatus(rr, httptest.NewRequest(http.MethodGet, "/payments/session-status?session_id=cs_123", nil))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var result map[string]interface{}
	if err := json.NewDecoder(rr.Body).Decode(&result); err != nil {
		t.Fatalf("failed to decode: %v", err)
	}
	if result["paid"] != false {
		t.Error("expected paid=false")
	}
}

func TestGetCheckoutSessionStatus_MissingMetadata(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		getCheckoutSessionFn: func(_ context.Context, _ string) (payments.CheckoutSessionPaid, error) {
			return payments.CheckoutSessionPaid{ID: "cs_123", Paid: true, Metadata: map[string]string{}}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.GetCheckoutSessionStatus(rr, httptest.NewRequest(http.MethodGet, "/payments/session-status?session_id=cs_123", nil))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestGetCheckoutSessionStatus_InvalidUID(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		getCheckoutSessionFn: func(_ context.Context, _ string) (payments.CheckoutSessionPaid, error) {
			return payments.CheckoutSessionPaid{
				ID: "cs_123", Paid: true,
				Metadata: map[string]string{"uid": "notanint", "package_id": "coin_pack_small", "coins": "1000"},
			}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.GetCheckoutSessionStatus(rr, httptest.NewRequest(http.MethodGet, "/payments/session-status?session_id=cs_123", nil))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestGetCheckoutSessionStatus_InvalidCoins(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		getCheckoutSessionFn: func(_ context.Context, _ string) (payments.CheckoutSessionPaid, error) {
			return payments.CheckoutSessionPaid{
				ID: "cs_123", Paid: true,
				Metadata: map[string]string{"uid": "1", "package_id": "coin_pack_small", "coins": "notanint"},
			}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.GetCheckoutSessionStatus(rr, httptest.NewRequest(http.MethodGet, "/payments/session-status?session_id=cs_123", nil))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestGetCheckoutSessionStatus_InvalidPackage(t *testing.T) {
	setupTestDB(t)
	setupMockPaymentClient(t, &mockPaymentClient{
		getCheckoutSessionFn: func(_ context.Context, _ string) (payments.CheckoutSessionPaid, error) {
			return payments.CheckoutSessionPaid{
				ID: "cs_123", Paid: true,
				Metadata: map[string]string{"uid": "1", "package_id": "invalid_pack", "coins": "1000"},
			}, nil
		},
	})
	rr := httptest.NewRecorder()
	cardservices.GetCheckoutSessionStatus(rr, httptest.NewRequest(http.MethodGet, "/payments/session-status?session_id=cs_123", nil))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── isDuplicateEventIDError (via HandlePaymentWebhook) ────────────────────────

func TestIsDuplicateEventIDError_Nil(t *testing.T) {
	// Covered indirectly — duplicate ledger entries return nil (idempotent)
	// This test verifies the webhook handles a second identical event gracefully
	db := setupTestDB(t)
	if err := db.AutoMigrate(&models.PaymentLedger{}); err != nil {
	t.Fatalf("failed to migrate: %v", err)
}

	setupMockPaymentClient(t, &mockPaymentClient{
		parseWebhookFn: func(_ []byte, _ http.Header) (payments.WebhookEvent, error) {
			return payments.WebhookEvent{
				ID:                "evt_dup",
				CheckoutCompleted: true,
				Session: &payments.CheckoutSessionPaid{
					ID:   "cs_dup",
					Paid: true,
					Metadata: map[string]string{
						"uid": "1", "package_id": "coin_pack_small", "coins": "1000",
					},
				},
			}, nil
		},
	})

	// First call
	rr1 := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr1, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))

	// Second call with same event — should be idempotent
	rr2 := httptest.NewRecorder()
	cardservices.HandlePaymentWebhook(rr2, httptest.NewRequest(http.MethodPost, "/payments/webhook", bytes.NewBufferString("{}")))
	if rr2.Code != http.StatusOK {
		t.Errorf("expected 200 for duplicate event, got %d", rr2.Code)
	}
}
