# speedcardgame

## How To Run Client
1. Run the following commands from /client

        mingw32-make
        
        ./main
2. If running on WSL, run the following commands from /client

        make -f Makefile.WSL

        ./main

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
