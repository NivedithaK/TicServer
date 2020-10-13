/* Implementation decisions:
 * - When player1 disconnects and there's no player3, the server
 *   pauses until another player connects. Messages get sent once
 *   the new person connects, or else player2 is talking to themselves.
 * - HOW TO START: 
 *   Client: nc hostname portnumber (e.g. nc localhost 5656)
 *   Server: ./a.out -p 5656 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

char board[10];
struct Player *head;
int current_move = 0;   // 0 = O's turn, 1 = X's turn

struct Player {
    int fd;
    char buf[100];
    int size;
    char *nextpos;
    int name;
    struct Player *next; 
};

int main(int argc, char **argv)
{
    extern void showboard(int fd);
    extern char *extractline(char *p, int size);
    extern void showboard(int fd);
    extern int isfull();
    extern int allthree(int start, int offset);
    extern int game_is_over();
    int port = 3000;
    int serverfd, clientfd, maxfd, c, status = 0;
    struct Player *temp;
    socklen_t len;
    struct sockaddr_in r, q;
    fd_set fds;
    memcpy(board, "123456789", 9);

    while ((c = getopt(argc, argv, "p:")) != EOF) {
        switch (c) {
        case 'p':
            port = atoi(optarg);
            break;
        default:
            status = 1;
        }
    }
    if (status || optind < argc) {
        fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
        return(1);
    }

    if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return(1);
    }

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);

    if (bind(serverfd, (struct sockaddr *)&r, sizeof r) < 0) {
        perror("bind");
        return(1);
    }
    if (listen(serverfd, 5)) {
        perror("listen");
        return(1);
    }

    len = sizeof q;
    if ((clientfd = accept(serverfd, (struct sockaddr *)&q, &len)) < 0) {
        perror("accept");
        return(1);
    }

    printf("new connection from %s\n", inet_ntoa(q.sin_addr));
    struct Player *player1 = malloc (sizeof(struct Player)); 
    player1->fd = clientfd;
    player1->size = 0;
    player1->next = NULL;
    player1->name = 1;
    head = player1;
    printf("client from %s is now o\n", inet_ntoa(q.sin_addr));
    static char msg[] = "You now get to play! You are now o.\r\n";
    write(player1->fd, msg, sizeof msg - 1);
    static char msg2[] = "It's your turn!\r\n";
    write(player1->fd, msg2, sizeof msg2 - 1);

    /*  Strategy: Iterate through linked list, adding everything to fd_set fds.
     *  Then call select, and make a move if the client is a player and makes a valid move.
     *  Otherwise, just write the whole message to everyone.
    */

    while (1) {
        int result = game_is_over();
        if (result != 0) {
            if (result == ' ') {
                printf("RESULT = %c\n", result);
                snprintf(msg, 44, "It's a draw! Starting new game...\r\n");
            }
            else {
                printf("WINNER = %c\n", result);
                snprintf(msg, 44, "Winner is %c! Starting new game...\r\n", result);
            }
            struct Player *round = head;
            while (round != NULL) {
                write(round->fd, msg, strlen(msg));
                round = round->next;
            }
            memcpy(board, "123456789", 9);
            round = head;
            while (round != NULL) {
                showboard(round->fd);
                round = round->next;
            }

            static char msg[] = "It's your turn!\r\n";
            if (current_move == 0)
                write(head->fd, msg, sizeof msg - 1);
            else
                write(head->next->fd, msg, sizeof msg - 1);
        }


        FD_ZERO(&fds);
        FD_SET(serverfd, &fds);
        maxfd = serverfd;

        struct Player *curr = head;
        while (curr != NULL) {
            if (curr->fd > maxfd)
                maxfd = curr->fd;
            FD_SET(curr->fd, &fds);
            curr = curr->next;
        }

        select(maxfd + 1, &fds, NULL, NULL, NULL);
        
        if (FD_ISSET(serverfd, &fds)) {
            int connfd;
            if ((connfd = accept(serverfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                return(1);
            }
            printf("new connection from %s\n", inet_ntoa(q.sin_addr));
            if (head->next == NULL) {
                struct Player *player2 = malloc (sizeof(struct Player)); 
                player2->fd = connfd;
                player2->size= 0;
                player2->next = NULL;
                player2->name = 2;
                head->next = player2;
                struct Player *curr = head;
                while (curr != NULL) {
                    showboard(curr->fd);
                    curr = curr->next;
                }
                printf("client from %s is now x\n", inet_ntoa(q.sin_addr));
                static char msg[] = "You now get to play! You are now x.\r\n";
                write(player2->fd, msg, sizeof msg - 1);
            }
            else if (head == NULL) {
                struct Player *player1 = malloc (sizeof(struct Player)); 
                player1->fd = connfd;
                player1->size= 0;
                player1->next = temp;
                player1->name = 1;
                head->next = player1;
                head = player1;
                printf("client from %s is now o\n", inet_ntoa(q.sin_addr));
                static char msg[] = "You now get to play! You are now o.\r\n";
                write(player1->fd, msg, sizeof msg - 1);
            }

            else {
                struct Player *newplayer = malloc (sizeof(struct Player)); 
                newplayer->fd = connfd;
                newplayer->size= 0;
                newplayer->next = NULL;
                int i = 1;
                struct Player *curr = head;
                while (curr->next != NULL) {
                    curr = curr->next;
                    i++;
                }
                curr->next = newplayer;
                newplayer->name = i+1;
                showboard(newplayer->fd);
            }
        }

        curr = head;
        while (curr != NULL) {
            if (FD_ISSET(curr->fd, &fds)) {
                curr->size = read(curr->fd, curr->buf, 99);
                (curr->buf)[99] = '\0';
                if (curr->size == 0) { 
                    if (head->next != NULL && head->next->next != NULL) {
                        if (curr == head) {     // P1 disconnected
                            static char msg[] = "P1 disconnected. You are now player o!\r\n";
                            write(head->next->next->fd, msg, sizeof msg - 1);
                            struct Player *temp = head->next->next;
                            if (temp->next != NULL) 
                                head->next->next = temp->next;
                            else {
                                head->next->next = NULL;
                            }
                            temp->next = head->next;
                            head = temp;
                            curr = head;
                        }
                        else if (curr == head->next) {     // P2 disconnected
                            static char msg[] = "P2 disconnected. You are now player x!\r\n";
                            write(head->next->next->fd, msg, sizeof msg - 1);
                            struct Player *temp = head->next;
                            head->next = head->next->next;
                            free(temp);
                        }
                    }
                    else {
                        temp = curr->next;
                        curr = NULL;
                        break;
                    }
                }

                if (curr == head && current_move == 0 && curr->size < 3 && isdigit((curr->buf)[0])) {      // PLAYER 1 = 'O'
                    int move = (curr->buf)[0] - '0';
                    if (isdigit(board[move-1])) {
                        board[move-1] = 'O';
                        current_move = 1;
                        struct Player *round = head;
                        while (round != NULL) {
                            showboard(round->fd);
                            round = round->next;
                        }
                        static char msg[] = "It's your turn!\r\n";
                        write(head->next->fd, msg, sizeof msg - 1);
                    }
                    else {
                        static char msg[] = "You can't move there! Try again.\r\n";
                        write(head->fd, msg, sizeof msg - 1);
                    }
                }

                else if (curr == head && current_move == 1 && curr->size < 3 && isdigit((curr->buf)[0])) {
                    static char msg[] = "It's not your move yet.\r\n";
                    write(head->fd, msg, sizeof msg - 1);
                }

                else if (curr == head->next && current_move == 1 && curr->size < 3 && isdigit((curr->buf)[0])) {      // PLAYER 2 = 'X'
                    int move = (curr->buf)[0] - '0';
                    if (isdigit(board[move-1])) {
                        board[move-1] = 'X';
                        current_move = 0;
                        struct Player *round = head;
                        while (round != NULL) {
                            showboard(round->fd);
                            round = round->next;
                        }
                        static char msg[] = "It's your turn!\r\n";
                        write(head->fd, msg, sizeof msg - 1);
                    }
                    else {
                        static char msg[] = "You can't move there! Try again.\r\n";
                        write(head->next->fd, msg, sizeof msg - 1);
                    }
                }

                else if (curr == head->next && current_move == 1 && curr->size < 3 && isdigit((curr->buf)[0])) {
                    static char msg[] = "It's not your move yet.\r\n";
                    write(head->next->fd, msg, sizeof msg - 1);
                }

                else {
                    while ((curr->nextpos = extractline(curr->buf, curr->size))) {
                        char msg[120];
                        snprintf(msg, sizeof(char) * 120, "P%d says: %s\n", curr->name, curr->buf);
                        if (curr == head && current_move == 1 && strlen(msg) < 12 && isdigit(msg[9])) {
                            snprintf(msg, sizeof(char) * 120, "It's not your turn yet.\n");
                            write(curr->fd, msg, strlen(msg));
                        }
                        else if (curr == head->next && current_move == 0 && strlen(msg) < 12 && isdigit(msg[9])) {
                            snprintf(msg, sizeof(char) * 120, "It's not your turn yet.\n");
                            write(curr->fd, msg, strlen(msg));
                        }
                        else {
                            struct Player *round = head;
                            while (round != NULL) {
                                write(round->fd, msg, strlen(msg));
                                round = round->next;
                            }
                        }
                        curr->size -= curr->nextpos - curr->buf;
                        memmove(curr->buf, curr->nextpos, curr->size);
                    }
                }
            }
            curr = curr->next;
        }
    }
    return(0);
}


void showboard(int fd)
{
    char buf[100], *bufp, *boardp;
    int col, row;

    if (write(fd, "\r\n", strlen("\r\n")) != strlen("\r\n"))
        perror("write");

    for (bufp = buf, col = 0, boardp = board; col < 3; col++) {
        for (row = 0; row < 3; row++, bufp += 4)
            sprintf(bufp, " %c |", *boardp++);
        bufp -= 2;  // kill last " |"
        strcpy(bufp, "\r\n---+---+---\r\n");
        bufp = strchr(bufp, '\0');
    }
    if (write(fd, buf, bufp - buf) != bufp-buf)
        perror("write");


    char msg[30];
    if (current_move == 1) 
        strcpy(msg, "It is x's turn.\r\n");
    else
        strcpy(msg, "It is o's turn.\r\n");

    if (write(fd, msg, strlen(msg)) != strlen(msg))
        perror("write");
}


int game_is_over()  /* returns winner, or ' ' for draw, or 0 for not over */
{
    int i, c;
    extern int allthree(int start, int offset);
    extern int isfull();

    for (i = 0; i < 3; i++)
        if ((c = allthree(i, 3)) || (c = allthree(i * 3, 1)))
            return(c);
    if ((c = allthree(0, 4)) || (c = allthree(2, 2)))
        return(c);
    if (isfull())
        return(' ');
    return(0);
}

int allthree(int start, int offset)
{
    if (board[start] > '9' && board[start] == board[start + offset]
            && board[start] == board[start + offset * 2])
        return(board[start]);
    return(0);
}

int isfull()
{
    int i;
    for (i = 0; i < 9; i++)
        if (board[i] < 'A')
            return(0);
    return(1);
}

char *extractline(char *p, int size)
	/* returns pointer to string after, or NULL if there isn't an entire
	 * line here.  If non-NULL, original p is now a valid string. */
{
    int nl;
    for (nl = 0; nl < size && p[nl] != '\r' && p[nl] != '\n'; nl++)
	    ;
    if (nl == size)
	    return(NULL);

    /*
     * There are three cases: either this is a lone \r, a lone \n, or a CRLF.
     */
    if (p[nl] == '\r' && nl + 1 < size && p[nl+1] == '\n') {
	    /* CRLF */
	    p[nl] = '\0';
	    return(p + nl + 2);
    } 
    else {
	    /* lone \n or \r */
	    p[nl] = '\0';
	    return(p + nl + 1);
    }
}
