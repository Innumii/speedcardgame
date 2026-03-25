package payments

import (
	"context"
	"encoding/json"
	"fmt"

	"github.com/stripe/stripe-go/v83"
	"github.com/stripe/stripe-go/v83/charge"
	"github.com/stripe/stripe-go/v83/checkout/session"
	"github.com/stripe/stripe-go/v83/token"
	"github.com/stripe/stripe-go/v83/webhook"
)

type CheckoutSessionRequest struct {
	UserID     int
	CoinPack   CoinPackage
	SuccessURL string
	CancelURL  string
}

type CheckoutSessionResponse struct {
	ID  string `json:"id"`
	URL string `json:"url"`
}

type DirectCardPaymentRequest struct {
	UserID     int
	CoinPack   CoinPackage
	CardNumber string
	ExpMonth   int64
	ExpYear    int64
	CVC        string
	Cardholder string
}

type DirectCardPaymentResponse struct {
	ID     string `json:"id"`
	Status string `json:"status"`
	Paid   bool   `json:"paid"`
}

type CheckoutSessionPaid struct {
	ID           string            `json:"id"`
	PaymentState string            `json:"payment_state"`
	Metadata     map[string]string `json:"metadata"`
}

type WebhookEvent struct {
	ID      string               `json:"id"`
	Type    string               `json:"type"`
	Session *CheckoutSessionPaid `json:"session,omitempty"`
}

type Client interface {
	CreateCheckoutSession(ctx context.Context, req CheckoutSessionRequest) (CheckoutSessionResponse, error)
	ProcessCardPayment(ctx context.Context, req DirectCardPaymentRequest) (DirectCardPaymentResponse, error)
	ParseWebhook(payload []byte, signature string) (WebhookEvent, error)
}

type StripeClient struct {
	webhookSecret string
}

func NewStripeClient(apiKey, webhookSecret string) *StripeClient {
	stripe.Key = apiKey
	return &StripeClient{webhookSecret: webhookSecret}
}

func (c *StripeClient) CreateCheckoutSession(_ context.Context, req CheckoutSessionRequest) (CheckoutSessionResponse, error) {
	params := &stripe.CheckoutSessionParams{
		SuccessURL: stripe.String(req.SuccessURL),
		CancelURL:  stripe.String(req.CancelURL),
		Mode:       stripe.String(string(stripe.CheckoutSessionModePayment)),
		LineItems: []*stripe.CheckoutSessionLineItemParams{
			{
				Quantity: stripe.Int64(1),
				PriceData: &stripe.CheckoutSessionLineItemPriceDataParams{
					Currency:   stripe.String(req.CoinPack.Currency),
					UnitAmount: stripe.Int64(req.CoinPack.AmountCents),
					ProductData: &stripe.CheckoutSessionLineItemPriceDataProductDataParams{
						Name: stripe.String(req.CoinPack.Name),
					},
				},
			},
		},
		Metadata: map[string]string{
			"uid":        fmt.Sprintf("%d", req.UserID),
			"package_id": req.CoinPack.ID,
			"coins":      fmt.Sprintf("%d", req.CoinPack.Coins),
		},
	}

	cs, err := session.New(params)
	if err != nil {
		return CheckoutSessionResponse{}, err
	}

	return CheckoutSessionResponse{ID: cs.ID, URL: cs.URL}, nil
}

func (c *StripeClient) ProcessCardPayment(_ context.Context, req DirectCardPaymentRequest) (DirectCardPaymentResponse, error) {
	tok, err := token.New(&stripe.TokenParams{
		Card: &stripe.CardParams{
			Number:   stripe.String(req.CardNumber),
			ExpMonth: stripe.String(fmt.Sprintf("%d", req.ExpMonth)),
			ExpYear:  stripe.String(fmt.Sprintf("%d", req.ExpYear)),
			CVC:      stripe.String(req.CVC),
			Name:     stripe.String(req.Cardholder),
		},
	})
	if err != nil {
		return DirectCardPaymentResponse{}, err
	}

	chargeParams := &stripe.ChargeParams{
		Amount:      stripe.Int64(req.CoinPack.AmountCents),
		Currency:    stripe.String(req.CoinPack.Currency),
		Description: stripe.String(req.CoinPack.Name),
		Metadata: map[string]string{
			"uid":        fmt.Sprintf("%d", req.UserID),
			"package_id": req.CoinPack.ID,
			"coins":      fmt.Sprintf("%d", req.CoinPack.Coins),
		},
	}
	chargeParams.SetSource(tok.ID)

	ch, err := charge.New(chargeParams)
	if err != nil {
		return DirectCardPaymentResponse{}, err
	}

	return DirectCardPaymentResponse{
		ID:     ch.ID,
		Status: string(ch.Status),
		Paid:   ch.Paid,
	}, nil
}

func (c *StripeClient) ParseWebhook(payload []byte, signature string) (WebhookEvent, error) {
	event, err := webhook.ConstructEvent(payload, signature, c.webhookSecret)
	if err != nil {
		return WebhookEvent{}, err
	}

	out := WebhookEvent{ID: event.ID, Type: string(event.Type)}
	if event.Type != stripe.EventTypeCheckoutSessionCompleted {
		return out, nil
	}

	var cs stripe.CheckoutSession
	if err := json.Unmarshal(event.Data.Raw, &cs); err != nil {
		return WebhookEvent{}, err
	}

	out.Session = &CheckoutSessionPaid{
		ID:           cs.ID,
		PaymentState: string(cs.PaymentStatus),
		Metadata:     cs.Metadata,
	}

	return out, nil
}
