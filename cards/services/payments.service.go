package services

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"

	"github.com/FYL-Studios/speedcardgame/cards/config"
	"github.com/FYL-Studios/speedcardgame/cards/models"
	"github.com/FYL-Studios/speedcardgame/cards/payments"
	"github.com/FYL-Studios/speedcardgame/cards/util"
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

func InitializePaymentsFromEnv() error {
	client, err := payments.NewClientFromEnv(context.Background(), util.IsAwsEnabled())
	if err != nil {
		paymentClient = nil
		return err
	}

	paymentClient = client
	return nil
}

// SetPaymentClientForTests swaps the payment client implementation in unit tests.
func SetPaymentClientForTests(client payments.Client) {
	paymentClient = client
}

// ListCoinPackages godoc
// @Summary      List coin packages
// @Description  Returns all available coin packages for purchase
// @Tags         payments
// @Produce      json
// @Success      200  {object}  map[string]interface{}
// @Router       /payments/packages [get]
func ListCoinPackages(w http.ResponseWriter, _ *http.Request) {
	util.RespondWithJSON(w, http.StatusOK, map[string]interface{}{"packages": payments.ListCoinPackages()})
}

// CreateCoinCheckoutSession godoc
// @Summary      Create checkout session
// @Description  Creates a Stripe checkout session for a coin package purchase
// @Tags         payments
// @Accept       json
// @Produce      json
// @Param        body  body      createCoinCheckoutRequest  true  "Checkout session details"
// @Success      200   {object}  map[string]interface{}
// @Failure      400   {string}  string  "Invalid input or unsupported package"
// @Failure      502   {string}  string  "Failed to create checkout session"
// @Failure      503   {string}  string  "Payments module not configured"
// @Router       /payments/checkout-session [post]
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

// ProcessCardPayment godoc
// @Summary      Process direct card payment
// @Description  Processes a direct card payment for a coin package. Only available when ALLOW_DIRECT_CARD_PAYMENTS is enabled.
// @Tags         payments
// @Accept       json
// @Produce      json
// @Param        body  body      processCardPaymentRequest  true  "Card payment details"
// @Success      200   {object}  map[string]interface{}
// @Failure      400   {string}  string  "Invalid input or unsupported package"
// @Failure      402   {string}  string  "Payment not completed"
// @Failure      403   {string}  string  "Direct card payments disabled"
// @Failure      502   {string}  string  "Failed to process payment"
// @Failure      503   {string}  string  "Payments module not configured"
// @Router       /payments/card [post]
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

// HandlePaymentWebhook godoc
// @Summary      Handle payment webhook
// @Description  Receives and processes Stripe webhook events. On successful checkout, credits coins to the user's inventory.
// @Tags         payments
// @Accept       json
// @Produce      json
// @Param        Stripe-Signature  header    string  true  "Stripe webhook signature"
// @Success      200               {object}  map[string]string
// @Failure      400               {string}  string  "Invalid webhook payload or metadata"
// @Failure      500               {string}  string  "Failed to apply coin purchase"
// @Failure      503               {string}  string  "Payments module not configured"
// @Router       /payments/webhook [post]
func HandlePaymentWebhook(w http.ResponseWriter, r *http.Request) {
	if paymentClient == nil {
		util.RespondWithError(w, http.StatusServiceUnavailable, "payments module is not configured")
		return
	}

	payload, err := io.ReadAll(r.Body)
	if err != nil {
		util.RespondWithError(w, http.StatusBadRequest, "failed to read request body")
		return
	}

	event, err := paymentClient.ParseWebhook(payload, r.Header)
	if err != nil {
		util.RespondWithError(w, http.StatusBadRequest, fmt.Sprintf("failed to parse webhook: %v", err))
		return
	}

	if !event.CheckoutCompleted {
		util.RespondWithJSON(w, http.StatusOK, map[string]string{"received": "true"})
		return
	}

	if event.Session == nil {
		util.RespondWithError(w, http.StatusBadRequest, "session data missing from webhook")
		return
	}

	session := event.Session
	if !session.Paid {
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

// HandleStripeWebhook is kept as a compatibility wrapper for existing routes and tests.
func HandleStripeWebhook(w http.ResponseWriter, r *http.Request) {
	HandlePaymentWebhook(w, r)
}

// GetCheckoutSessionStatus godoc
// @Summary      Get checkout session status
// @Description  Checks the status of a Stripe checkout session and credits coins if payment is confirmed
// @Tags         payments
// @Produce      json
// @Param        session_id  query     string  true  "Stripe checkout session ID"
// @Success      200         {object}  map[string]interface{}
// @Failure      400         {string}  string  "Missing or invalid session data"
// @Failure      502         {string}  string  "Failed to retrieve session"
// @Failure      503         {string}  string  "Payments module not configured"
// @Router       /payments/checkout-session/status [get]
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

	if !session.Paid {
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
