package payments

import (
	"context"
	"fmt"
	"sync/atomic"
)

// MockPaymentClient provides a mock implementation for testing without Stripe
type MockPaymentClient struct{}

var mockSequence uint64

func NewMockPaymentClient() Client {
	return &MockPaymentClient{}
}

func (m *MockPaymentClient) CreateCheckoutSession(_ context.Context, req CheckoutSessionRequest) (CheckoutSessionResponse, error) {
	seq := atomic.AddUint64(&mockSequence, 1)
	return CheckoutSessionResponse{
		ID:  fmt.Sprintf("cs_mock_%d_%s_%d", req.UserID, req.CoinPack.ID, seq),
		URL: "https://checkout.stripe.com/pay/cs_mock",
	}, nil
}

func (m *MockPaymentClient) ProcessCardPayment(_ context.Context, req DirectCardPaymentRequest) (DirectCardPaymentResponse, error) {
	seq := atomic.AddUint64(&mockSequence, 1)
	// Simulate successful payment for test cards
	return DirectCardPaymentResponse{
		ID:     fmt.Sprintf("ch_mock_%d_%s_%d", req.UserID, req.CoinPack.ID, seq),
		Status: "succeeded",
		Paid:   true,
	}, nil
}

func (m *MockPaymentClient) ParseWebhook(payload []byte, signature string) (WebhookEvent, error) {
	return WebhookEvent{
		ID:   "evt_mock",
		Type: "checkout.session.completed",
	}, nil
}
