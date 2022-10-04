#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct s_client {
	int 	id;
	int 	fd;
	int		new_msg;
	struct s_client *next;
} t_client;

int max_fd = 0;
int id = 0;
fd_set all_sock, sending_sock, recv_sock;
t_client *client_list = NULL;

void fatal() {
	write(2, "Fatal error\n" , strlen("Fatal error\n"));
	t_client *current = NULL;
	while (client_list)
	{
		close(client_list->fd);
		current = client_list;
		client_list = client_list->next;
		free(current);
	}
	exit(1);
}


t_client *add_client_to_list(int new_connection) {
	//create new_client 
	t_client *new_client = (t_client *)calloc(1, sizeof(t_client));
	if (!new_client)
		fatal();

	//enter client info id and fd
	new_client->fd = new_connection;
	new_client->id = id;
	new_client->next = NULL;
	new_client->new_msg = 1;

	//look for last client
	t_client *last = client_list;
	if (last == NULL)
		return new_client;
	else {
		while (last->next)
			last = last->next;
		last->next = new_client;
	}
	return (client_list);
}


void broadcast(int sender_fd, char *raw_msg, int id) {
	char msg[200];
	sprintf(msg, raw_msg, id);

	//iterate over client list, looking for ones in recv_sock
		//if not sender then send msg to client
	t_client *current_client = client_list;
	while (current_client) {
		if (FD_ISSET(current_client->fd, &recv_sock) && current_client->fd != sender_fd)
			send(current_client->fd, msg, strlen(msg), 0);
		current_client = current_client->next;
	}
}

void accept_connection(int serv_sock) {
	//	accept connection
	int new_connection;
	new_connection = accept(serv_sock, NULL, NULL);
	if (new_connection < 0) 
		fatal();

	//	add new connection to all_sock
	FD_SET(new_connection, &all_sock);

	//	if fd is > than max_fd then adjust max_fd;
	if (new_connection > max_fd)
		max_fd = new_connection;

	//	add new connection to list of clients
	client_list = add_client_to_list(new_connection);

	//  Send "server: client %d just arrived\n"
	broadcast(new_connection,"server: client %d just arrived\n", id);

	id++;
}

t_client *remove_client(t_client *current_client) 
{
	FD_CLR(current_client->fd, &all_sock);
	close(current_client->fd);
	t_client *tmp = client_list;

	if (client_list == current_client) {
		client_list = current_client->next;
		free(current_client);
	}
	else
	{
		while (tmp && tmp->next != current_client)
			tmp = tmp->next;
		tmp->next = tmp->next->next;
		free(current_client);
	}
	return (client_list);
}

void handle_msgs() {
	//iterate over client_list -> when client is in sending_sock
    t_client *current_client = client_list;
    while (current_client)
	{
        if (FD_ISSET(current_client->fd, &sending_sock)) 
		{
			//receive msg from current_client char by char
			char c;
			int nb_char;
			nb_char = recv(current_client->fd, &c, 1, 0);

			//client disconnected
			if (nb_char <= 0) {
				broadcast(current_client->fd, "server: client %d just left\n", current_client->id);
				client_list = remove_client(current_client);
			}
			else 
			{
				if (current_client->new_msg)
				{
					broadcast(current_client->fd, "client %d: ", current_client->id);
					current_client->new_msg = 0;
				}
				if (c == '\n')
					current_client->new_msg = 1;
				broadcast(current_client->fd, &c, -1);
			}
		}
        current_client = current_client->next;
    }	
}

void run_server(int serv_sock) {
	//global : fd_set all_sock sending_sock, recv_sock;
	FD_ZERO(&all_sock);//NE PAS OUBLIER
	FD_SET(serv_sock, &all_sock);
	max_fd = serv_sock;

	while (1) {
		sending_sock = recv_sock = all_sock;

		//once select returns - sending_sock only contains the read-ready sockets and deleted the others
		//When an FD is "ready for reading", means we can READ FROM it
		//When an FD is "ready for writing", means we can WRITE TO it
		if (select(max_fd + 1, &sending_sock, &recv_sock, NULL, NULL) < 0)
			fatal();//or continue ?
		if (FD_ISSET(serv_sock, &sending_sock))
			accept_connection(serv_sock);
		else
			handle_msgs();
	}
}

int main(int argc, char **argv) {
	if (argc < 2)
	{
		write(2, "Wrong number of arguments\n" , strlen("Wrong number of arguments\n"));
		exit(1);
	}

	int sockfd;
	struct sockaddr_in servaddr; 

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal();	
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); //ATOI here

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();	
	if (listen(sockfd, 10) != 0) 
		fatal();

	run_server(sockfd);
	return (0);
}
