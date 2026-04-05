# speedcardgame

## How To Run Client
1. Run the following commands from /client

        mingw32-make
        
        ./main
2. If running on WSL, run the following commands from /client

        make -f Makefile.WSL

        ./main

### Client service endpoint toggle

The client now defaults to AWS service endpoints.

- `ENABLE_ANIMATIONS=true` (default): enables menu and gameplay animations.
- `ENABLE_ANIMATIONS=false`: disables all queued gameplay animations and menu intro motion.

- `USE_AWS_SERVICES=true` (default): use AWS endpoint env vars
        - `AWS_AUTH_SERVICE_HOST` / `AWS_AUTH_SERVICE_PORT` (default port `443`)
        - `AWS_CARDS_SERVICE_HOST` / `AWS_CARDS_SERVICE_PORT` (default port `443`)
        - `AWS_GAME_SERVER_SERVICE_HOST` / `AWS_GAME_SERVER_SERVICE_PORT` (default port `4000`)
- `USE_AWS_SERVICES=false`: use local endpoint env vars
        - `AUTH_SERVICE_HOST` / `AUTH_SERVICE_PORT` (defaults `127.0.0.1:8081`)
        - `CARDS_SERVICE_HOST` / `CARDS_SERVICE_PORT` (defaults `127.0.0.1:8082`)
        - `GAME_SERVER_SERVICE_HOST` / `GAME_SERVER_SERVICE_PORT` (defaults `127.0.0.1:4000`)

Example (PowerShell):

```powershell
$env:USE_AWS_SERVICES = "true"
$env:AWS_AUTH_SERVICE_HOST = "api.myapp.com"
$env:AWS_CARDS_SERVICE_HOST = "api.myapp.com"
$env:AWS_GAME_SERVER_SERVICE_HOST = "<your-server-host>"
./main
```

2. Note that with each new .cpp added to the repo, it must also be added to the Makefile (will eventually switch to a better compiling process but for now just use this)

## How to Run Server
1. Run the following commands from /server (ensure you are using WSL in your VSCode)

        make
        
        ./server

## How to Run Cards Service
1. cd to the cards folder

        cd cards

2. Open Docker and run the following command

        docker-compose up -d --build

- Note: Please copy and fill in the .env located in cards

## HTTP and debug logging toggles

The `auth` and `cards` services support runtime logging toggles through environment variables:

- `HTTP_REQUEST_LOG_ENABLED` (default `true`): enables structured HTTP request logging.
- `DEBUG_LOG_ENABLED` (default `false`): enables verbose request diagnostics (user-agent and referer).

These vars work in local `docker-compose` and deployed ECS tasks (via Terraform).

## Testing

### Unit Tests

#### Auth
```bash
cd auth
go get github.com/alicebob/miniredis/v2 && go mod tidy  # first time only
go test ./test/unit/... -p=1 -count=1 -v
```

With coverage:
```bash
go test ./test/unit/... -p=1 -count=1 -coverprofile=coverage.out -coverpkg=github.com/FYL-Studios/speedcardgame/auth/services/...
go tool cover -func=coverage.out       # summary
go tool cover -html=coverage.out -o coverage.html  # HTML report
```

#### Cards
```bash
cd cards
go get gorm.io/driver/sqlite github.com/mattn/go-sqlite3 && go mod tidy  # first time only
go test ./test/unit/... -p=1 -count=1 -v
```

With coverage:
```bash
go test ./test/unit/... -p=1 -count=1 -coverprofile=coverage.out -coverpkg=github.com/FYL-Studios/speedcardgame/cards/services/...
go tool cover -func=coverage.out
go tool cover -html=coverage.out -o coverage.html
```

---

### Integration Tests

Requires Docker Compose running first:
```bash
docker-compose up -d auth-db auth-redis cards-db
```

#### Auth
```bash
cd auth
go test ./test/integration/... -v -tags=integration -p=1 -count=1
```

#### Cards
```bash
cd cards
go test ./test/integration/... -v -tags=integration -p=1 -count=1
```

> **Note:** Integration tests connect to real Postgres and Redis instances. Make sure your `.env` file is present in both `auth/` and `cards/` before running.
## Swagger Documentation

### Prerequisites
- [swag CLI](https://github.com/swaggo/swag) installed globally:
```bash
  go install github.com/swaggo/swag/cmd/swag@latest
```

---

### Generating the Docs

Run this whenever you update any Swagger annotations:
```bash
# Auth
cd auth
swag init --parseDependency --parseInternal

# Cards
cd cards
swag init --parseDependency --parseInternal
```

This generates/updates the `docs/` folder in each service.

---

### Viewing the Docs

Start the services:
```bash
docker-compose up auth cards
```

Then open in your browser:

| Service | URL |
|---------|-----|
| Auth    | http://localhost:8081/swagger/index.html |
| Cards   | http://localhost:8082/cards/swagger/index.html |

---

### Notes
- The `docs/` folder is auto-generated — do not edit it manually
- Re-run `swag init` after adding or changing any `@Summary`, `@Router` or other Swagger annotations
- The `docs/` folder should be committed to version control so the app builds correctly in Docker


