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
