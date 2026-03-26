package services

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/Ryanljk/speedcardgame/cards/payments"
	"github.com/Ryanljk/speedcardgame/cards/util"
	"github.com/aws/aws-sdk-go-v2/aws"
	awsconfig "github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/secretsmanager"
	"gorm.io/gorm"
	"gorm.io/gorm/clause"
)

var paymentClient payments.Client

type createCoinCheckoutRequest struct {
	Uid        int    `json:"uid"`
	PackageID  string `json:"package_id"`
	SuccessURL string `json:"success_url"`
	CancelURL  string `json:"cancel_url"`
}

type processCardPaymentRequest struct {
	Uid            int    `json:"uid"`
	PackageID      string `json:"package_id"`
	CardNumber     string `json:"card_number"`
	ExpMonth       int64  `json:"exp_month"`
	ExpYear        int64  `json:"exp_year"`
	CVC            string `json:"cvc"`
	CardholderName string `json:"cardholder_name"`
}

// stripeSecrets holds the Stripe fields within the shared cards runtime secret.
type stripeSecrets struct {
    SecretKey     string `json:"STRIPE_SECRET_KEY"`
    WebhookSecret string `json:"STRIPE_WEBHOOK_SECRET"`
}

func loadStripeSecretsFromAWS(ctx context.Context) (stripeSecrets, error) {
    // Use the shared cards runtime secret — defaults match the Terraform default
    // in infra/modules/secrets/variables.tf (cards_runtime_secret_name).
    secretName := util.GetEnvOrDefault("CARDS_RUNTIME_SECRET_NAME", "speedcardgame-cards-runtime")

    cfg, err := awsconfig.LoadDefaultConfig(ctx)
    if err != nil {
        return stripeSecrets{}, fmt.Errorf("failed to load AWS config: %w", err)
    }

    client := secretsmanager.NewFromConfig(cfg)
    result, err := client.GetSecretValue(ctx, &secretsmanager.GetSecretValueInput{
        SecretId: aws.String(secretName),
    })
    if err != nil {
        return stripeSecrets{}, fmt.Errorf("failed to get secret %q: %w", secretName, err)
    }

    if result.SecretString == nil {
        return stripeSecrets{}, fmt.Errorf("secret %q has no string value", secretName)
    }

    var s stripeSecrets
    if err := json.Unmarshal([]byte(*result.SecretString), &s); err != nil {
        return stripeSecrets{}, fmt.Errorf("failed to parse secret JSON: %w", err)
    }

    return s, nil
}

func InitializePaymentsFromEnv() error {
    // 1. Mock client for tests / CI
    if os.Getenv("PAYMENT_TEST_MODE") == "true" {
        paymentClient = payments.NewMockPaymentClient()
        return nil
    }

    // 2. Local dev: explicit opt-in via USE_LOCAL_SECRETS=true
    if !util.IsAwsEnabled(){
        apiKey := os.Getenv("STRIPE_SECRET_KEY")
        webhookSecret := os.Getenv("STRIPE_WEBHOOK_SECRET")

        if apiKey == "" || webhookSecret == "" {
            paymentClient = nil
            return errors.New(
                "stripe payment module disabled: USE_LOCAL_SECRETS=true but " +
                    "STRIPE_SECRET_KEY or STRIPE_WEBHOOK_SECRET is not set",
            )
        }

        paymentClient = payments.NewStripeClient(apiKey, webhookSecret)
        return nil
    }

    // 3. Default: AWS Secrets Manager using the shared cards runtime secret
    secrets, err := loadStripeSecretsFromAWS(context.Background())
    if err != nil {
        paymentClient = nil
        return fmt.Errorf(
            "stripe payment module disabled: failed to load from cards runtime secret: %w", err,
        )
    }

    if secrets.SecretKey == "" || secrets.WebhookSecret == "" {
        paymentClient = nil
        return errors.New(
            "stripe payment module disabled: cards runtime secret missing STRIPE_SECRET_KEY or STRIPE_WEBHOOK_SECRET",
        )
    }

    paymentClient = payments.NewStripeClient(secrets.SecretKey, secrets.WebhookSecret)
    return nil
}

// SetPaymentClientForTests swaps the payment client implementation in unit tests.
func SetPaymentClientForTests(client payments.Client) {
	paymentClient = client
}

func ListCoinPackages(w http.ResponseWriter, _ *http.Request) {
	util.RespondWithJSON(w, http.StatusOK, map[string]interface{}{"packages": payments.ListCoinPackages()})
}

func CreateCoinCheckoutSession(w http.ResponseWriter, r *http.Request) {
	if paymentClient == nil {
		util.RespondWithError(w, http.StatusServiceUnavailable, "payments module is not configured")
		return
	}

	var req createCoinCheckoutRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		util.RespondWithError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	if req.Uid <= 0 {
		util.RespondWithError(w, http.StatusBadRequest, "uid must be a positive integer")
		return
	}

	coinPack, ok := payments.GetCoinPackage(req.PackageID)
	if !ok {
		util.RespondWithError(w, http.StatusBadRequest, "unsupported package_id")
		return
	}

	successURL := req.SuccessURL
	if successURL == "" {
		successURL = util.ResolveCardsServiceBaseURL() + "/cards/payments/checkout-complete?status=success"
	}
	cancelURL := req.CancelURL
	if cancelURL == "" {
		cancelURL = util.ResolveCardsServiceBaseURL() + "/cards/payments/checkout-complete?status=cancel"
	}

	if successURL == "" || cancelURL == "" {
		util.RespondWithError(w, http.StatusBadRequest, "success_url and cancel_url are required")
		return
	}

	cs, err := paymentClient.CreateCheckoutSession(r.Context(), payments.CheckoutSessionRequest{
		UserID:     req.Uid,
		CoinPack:   coinPack,
		SuccessURL: successURL,
		CancelURL:  cancelURL,
	})
	if err != nil {
		util.RespondWithError(w, http.StatusBadGateway, fmt.Sprintf("failed to create checkout session: %v", err))
		return
	}

	util.RespondWithJSON(w, http.StatusOK, cs)
}

func RenderCheckoutCompletePage(w http.ResponseWriter, r *http.Request) {
	status := strings.ToLower(strings.TrimSpace(r.URL.Query().Get("status")))
	if status != "success" && status != "cancel" {
		status = "complete"
	}

	message := "Payment flow completed. You can return to the game."
	if status == "success" {
		message = "Payment successful. Returning to game..."
	} else if status == "cancel" {
		message = "Payment canceled. Returning to game..."
	}

	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	_, _ = fmt.Fprintf(w, `<!doctype html>
<html>
<head>
	<meta charset="utf-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1" />
	<title>Checkout Complete</title>
	<style>
		body { font-family: Segoe UI, Arial, sans-serif; margin: 0; background: #0f172a; color: #e2e8f0; }
		.box { max-width: 520px; margin: 10vh auto; background: #111827; padding: 24px; border-radius: 10px; border: 1px solid #334155; }
		.muted { color: #94a3b8; font-size: 14px; }
	</style>
</head>
<body>
	<div class="box">
		<h2>%s</h2>
		<p class="muted">If this tab does not close automatically, you can close it manually.</p>
	</div>
	<script>
		setTimeout(function () { window.close(); }, 150);
	</script>
</body>
</html>`, message)
}

func ProcessCardPayment(w http.ResponseWriter, r *http.Request) {
	if !util.GetEnvAsBool("ALLOW_DIRECT_CARD_PAYMENTS", false) {
		util.RespondWithError(w, http.StatusForbidden, "direct card payments are disabled; use /cards/payments/checkout-session")
		return
	}

	if paymentClient == nil {
		util.RespondWithError(w, http.StatusServiceUnavailable, "payments module is not configured")
		return
	}

	var req processCardPaymentRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		util.RespondWithError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	if req.Uid <= 0 || req.PackageID == "" || req.CardNumber == "" || req.CVC == "" || req.CardholderName == "" {
		util.RespondWithError(w, http.StatusBadRequest, "invalid payment request")
		return
	}

	coinPack, ok := payments.GetCoinPackage(req.PackageID)
	if !ok {
		util.RespondWithError(w, http.StatusBadRequest, "unsupported package_id")
		return
	}

	paymentRes, err := paymentClient.ProcessCardPayment(r.Context(), payments.DirectCardPaymentRequest{
		UserID:     req.Uid,
		CoinPack:   coinPack,
		CardNumber: req.CardNumber,
		ExpMonth:   req.ExpMonth,
		ExpYear:    req.ExpYear,
		CVC:        req.CVC,
		Cardholder: req.CardholderName,
	})
	if err != nil {
		util.RespondWithError(w, http.StatusBadGateway, fmt.Sprintf("failed to process card payment: %v", err))
		return
	}

	if !paymentRes.Paid {
		util.RespondWithError(w, http.StatusPaymentRequired, "payment was not completed")
		return
	}

	if err := applyCoinPurchase("direct:"+paymentRes.ID, paymentRes.ID, req.Uid, coinPack.Coins, coinPack.AmountCents, coinPack.Currency); err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("failed to apply coin purchase: %v", err))
		return
	}

	util.RespondWithJSON(w, http.StatusOK, map[string]interface{}{
		"payment":     paymentRes,
		"coins_added": coinPack.Coins,
	})
}

func HandleStripeWebhook(w http.ResponseWriter, r *http.Request) {
	if paymentClient == nil {
		util.RespondWithError(w, http.StatusServiceUnavailable, "payments module is not configured")
		return
	}

	payload, err := io.ReadAll(r.Body)
	if err != nil {
		util.RespondWithError(w, http.StatusBadRequest, "failed to read request body")
		return
	}

	event, err := paymentClient.ParseWebhook(payload, r.Header.Get("Stripe-Signature"))
	if err != nil {
		util.RespondWithError(w, http.StatusBadRequest, fmt.Sprintf("failed to parse webhook: %v", err))
		return
	}

	if event.Type != "checkout.session.completed" {
		util.RespondWithJSON(w, http.StatusOK, map[string]string{"received": "true"})
		return
	}

	if event.Session == nil {
		util.RespondWithError(w, http.StatusBadRequest, "session data missing from webhook")
		return
	}

	session := event.Session
	if session.PaymentState != "paid" {
		util.RespondWithJSON(w, http.StatusOK, map[string]string{"received": "true"})
		return
	}

	uidStr := session.Metadata["uid"]
	packageID := session.Metadata["package_id"]
	coinsStr := session.Metadata["coins"]

	if uidStr == "" || packageID == "" || coinsStr == "" {
		util.RespondWithError(w, http.StatusBadRequest, "missing required metadata in webhook")
		return
	}

	uid, err := strconv.Atoi(uidStr)
	if err != nil {
		util.RespondWithError(w, http.StatusBadRequest, "invalid uid in metadata")
		return
	}

	coins, err := strconv.Atoi(coinsStr)
	if err != nil {
		util.RespondWithError(w, http.StatusBadRequest, "invalid coins in metadata")
		return
	}

	coinPack, ok := payments.GetCoinPackage(packageID)
	if !ok {
		util.RespondWithError(w, http.StatusBadRequest, "unknown package_id in metadata")
		return
	}

	if err := applyCoinPurchase("checkout:"+session.ID, session.ID, uid, coins, coinPack.AmountCents, coinPack.Currency); err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("failed to apply coin purchase: %v", err))
		return
	}

	util.RespondWithJSON(w, http.StatusOK, map[string]string{"success": "true"})
}

func GetCheckoutSessionStatus(w http.ResponseWriter, r *http.Request) {
	if paymentClient == nil {
		util.RespondWithError(w, http.StatusServiceUnavailable, "payments module is not configured")
		return
	}

	sessionID := strings.TrimSpace(r.URL.Query().Get("session_id"))
	if sessionID == "" {
		util.RespondWithError(w, http.StatusBadRequest, "session_id is required")
		return
	}

	session, err := paymentClient.GetCheckoutSession(r.Context(), sessionID)
	if err != nil {
		util.RespondWithError(w, http.StatusBadGateway, fmt.Sprintf("failed to get checkout session: %v", err))
		return
	}

	if session.PaymentState != "paid" {
		util.RespondWithJSON(w, http.StatusOK, map[string]interface{}{
			"session_id": session.ID,
			"paid":       false,
		})
		return
	}

	uidStr := session.Metadata["uid"]
	packageID := session.Metadata["package_id"]
	coinsStr := session.Metadata["coins"]

	if uidStr == "" || packageID == "" || coinsStr == "" {
		util.RespondWithError(w, http.StatusBadRequest, "missing required metadata in checkout session")
		return
	}

	uid, err := strconv.Atoi(uidStr)
	if err != nil {
		util.RespondWithError(w, http.StatusBadRequest, "invalid uid in checkout session metadata")
		return
	}

	coins, err := strconv.Atoi(coinsStr)
	if err != nil {
		util.RespondWithError(w, http.StatusBadRequest, "invalid coins in checkout session metadata")
		return
	}

	coinPack, ok := payments.GetCoinPackage(packageID)
	if !ok {
		util.RespondWithError(w, http.StatusBadRequest, "unknown package_id in checkout session metadata")
		return
	}

	if err := applyCoinPurchase("checkout:"+session.ID, session.ID, uid, coins, coinPack.AmountCents, coinPack.Currency); err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("failed to apply coin purchase: %v", err))
		return
	}

	util.RespondWithJSON(w, http.StatusOK, map[string]interface{}{
		"session_id":  session.ID,
		"paid":        true,
		"coins_added": coins,
	})
}

func applyCoinPurchase(eventID, sessionID string, uid, coins int, amountCents int64, currency string) error {
	return config.DB.Transaction(func(tx *gorm.DB) error {
		ledger := models.PaymentLedger{
			EventID:     eventID,
			SessionID:   sessionID,
			Uid:         uid,
			Coins:       coins,
			AmountCents: amountCents,
			Currency:    currency,
		}

		if err := tx.Create(&ledger).Error; err != nil {
			if isDuplicateEventIDError(err) {
				return nil
			}
			return fmt.Errorf("failed to create payment ledger: %w", err)
		}

		inventory := models.Inventory{Uid: uid, Coins: coins}
		if err := tx.Clauses(clause.OnConflict{
			Columns: []clause.Column{{Name: "uid"}},
			DoUpdates: clause.Assignments(map[string]interface{}{
				"coins": gorm.Expr("inventories.coins + EXCLUDED.coins"),
			}),
		}).Create(&inventory).Error; err != nil {
			return fmt.Errorf("failed to upsert inventory: %w", err)
		}

		return nil
	})
}

func isDuplicateEventIDError(err error) bool {
	if err == nil {
		return false
	}

	msg := strings.ToLower(err.Error())
	if strings.Contains(msg, "duplicate key value") && strings.Contains(msg, "idx_payment_ledgers_event_id") {
		return true
	}

	return strings.Contains(msg, "unique constraint failed") && strings.Contains(msg, "payment_ledgers.event_id")
}