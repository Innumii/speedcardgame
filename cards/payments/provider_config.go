package payments

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strings"

	"github.com/FYL-Studios/speedcardgame/cards/util"
	awsconfig "github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/secretsmanager"
)

// NewClientFromEnv initializes a payment provider based on runtime configuration.
// PAYMENT_PROVIDER controls the selected gateway (default: stripe).
func NewClientFromEnv(ctx context.Context, awsEnabled bool) (Client, error) {
	if os.Getenv("PAYMENT_TEST_MODE") == "true" {
		return NewMockPaymentClient(), nil
	}

	provider := strings.ToLower(strings.TrimSpace(util.GetEnvOrDefault("PAYMENT_PROVIDER", "stripe")))
	switch provider {
	case "stripe":
		return newStripeClientFromEnv(ctx, awsEnabled)
	default:
		return nil, fmt.Errorf("unsupported PAYMENT_PROVIDER %q", provider)
	}
}

// stripeSecrets holds the Stripe fields within the shared cards runtime secret.
type stripeSecrets struct {
	SecretKey     string `json:"STRIPE_SECRET_KEY"`
	WebhookSecret string `json:"STRIPE_WEBHOOK_SECRET"`
}

func newStripeClientFromEnv(ctx context.Context, awsEnabled bool) (Client, error) {
	if !awsEnabled {
		apiKey := os.Getenv("STRIPE_SECRET_KEY")
		webhookSecret := os.Getenv("STRIPE_WEBHOOK_SECRET")

		if apiKey == "" || webhookSecret == "" {
			return nil, errors.New(
				"stripe payment module disabled: USE_LOCAL_SECRETS=true but " +
					"STRIPE_SECRET_KEY or STRIPE_WEBHOOK_SECRET is not set",
			)
		}

		return NewStripeClient(apiKey, webhookSecret), nil
	}

	secrets, err := loadStripeSecretsFromAWS(ctx)
	if err != nil {
		return nil, fmt.Errorf(
			"stripe payment module disabled: failed to load from cards runtime secret: %w", err,
		)
	}

	if secrets.SecretKey == "" || secrets.WebhookSecret == "" {
		return nil, errors.New(
			"stripe payment module disabled: cards runtime secret missing STRIPE_SECRET_KEY or STRIPE_WEBHOOK_SECRET",
		)
	}

	return NewStripeClient(secrets.SecretKey, secrets.WebhookSecret), nil
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
	output, err := client.GetSecretValue(ctx, &secretsmanager.GetSecretValueInput{SecretId: &secretName})
	if err != nil {
		return stripeSecrets{}, fmt.Errorf("failed to fetch secret %s: %w", secretName, err)
	}

	secretValue := output.SecretString
	if secretValue == nil {
		return stripeSecrets{}, fmt.Errorf("secret %s has no secret string", secretName)
	}

	var secrets stripeSecrets
	if err := json.Unmarshal([]byte(*secretValue), &secrets); err != nil {
		return stripeSecrets{}, fmt.Errorf("failed to unmarshal secret %s: %w", secretName, err)
	}

	return secrets, nil
}
