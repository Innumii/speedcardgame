# Getting started



1. Install [PostgreSQL](https://www.postgresql.org/download/)

2. Install [Redis](https://redis.io/docs/latest/operate/oss_and_stack/install/install-redis/install-redis-on-windows/)

- For Windows Installation, open PowerShell as Administrator:

```bash
wsl --install

wsl --set-default-version 2
```

- Install a Linux distribution such as Ubuntu, restart your computer
- Open Ubuntu

```bash
sudo apt update

sudo apt install redis-server

sudo service redis-server start
```

**Ensure both Redis and PostgreSQL are running**

3. Formatting
   Uses golangci for formatting, check .golangci.yml

- Without Makefile

```bash
gofmt -s -w .

golangci-lint run
```

- With [Makefile](## setting-up-makefile) (Windows)

```bash
make format
```

## Running the application

### With Make

```bash
make setup

make run dev

# drop all tables and clear Redis
make drop

# create all tables
make create

# build and run docker
make up
```

### Without Make

```bash
wgo run main.go

# drop all tables and clear Redis
go run main.go db:drop

# create all tables
go run main.go db:create

# to clean up modules
go mod tidy
```

- wgo runs the app in watch mode
- App will be running on <http://localhost:8080>

## Setting up Makefile

1. Install [Chocolatey](https://chocolatey.org/install#individual)

```bash
choco install make
```
