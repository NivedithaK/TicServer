# TicServer
Allows multiple users to connect and send chat messages and play tic-tac-toe remotely.

# How To Play

Server: 

gcc ticsvr.c -o ticsvr

./ticsvr -p portnumber (e.g. ./ticsvr -p 5656)

Client 1: 

nc hostname portnumber (e.g. nc localhost 5656)

Client 2: 

nc hostname portnumber (e.g. nc localhost 5656)

etc.

# Additional Details
- The first two people to connect will be Player 1 and Player 2 for the tic-tac-toe game
- All subsequent joining users can only observe the game and send chat messages
- If one of the players disconnect, then another user will take their place
- The user that takes their place is decided on a first-come, first-served basis
- The server receives notifications whenever any user connects and disconnects 

# Next Steps 
- Let users choose their usernames (easy fix)
- Connect to a front-end
