#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>


#define BAT0 "/sys/class/power_supply/BAT0/"
#define BAT0_FULL BAT0 "energy_full"
#define BAT0_NOW BAT0 "energy_now"
#define SERVER_PORT 33333
#define LISTEN_BACKLOG 10

#define REPLY_HEADERS "HTTP/1.0 200 OK\n"\
	"Server: battery-daemon\n"\
	"Content-Type: application/json\n"\
	"Content-Length: %i\n"\
	"Connection: close\n\n"
#define REPLY "{\"status\": %d, \"battery_charge\": %f}\n"
#define REPLY_BUFSZ 128


int long_from_file(const char* fn, long* ret)
{
	if (!ret) return 0;
	FILE* file = fopen(fn, "r");
	if (!file) return 0;

	long read;
	int status = fscanf(file, "%ld", &read);
	fclose(file);
	if (status < 1 || status == EOF) return 0;
	*ret = read;
	return 1;
}


int get_battery_charge(float* ret)
{
	if (!ret) return 0;
	long energy_full, energy_now;
	int status = long_from_file(BAT0_FULL, &energy_full);
	if (!status) return 0;
	status = long_from_file(BAT0_NOW, &energy_now);
	if (!status) return 0;
	*ret = (float)energy_now / (float)energy_full;
	return 1;
}


void* connection_handler(void* data)
{
	int sock_client = (int)data;
	char reply_headers[REPLY_BUFSZ], reply[REPLY_BUFSZ];
	float batt_chrage = 0.0f;
	int status = get_battery_charge(&batt_chrage);
	int reply_sz = snprintf(reply, REPLY_BUFSZ, REPLY, status, batt_chrage);
	if (reply_sz > 0 && reply_sz < REPLY_BUFSZ)
	{
		int reply_headers_sz = snprintf(reply_headers, REPLY_BUFSZ, REPLY_HEADERS, reply_sz);
		if (reply_headers_sz > 0 && reply_headers_sz < REPLY_BUFSZ)
		{
			write(sock_client, reply_headers, reply_headers_sz);
			write(sock_client, reply, reply_sz);
		}
	}
	close(sock_client);
	return NULL;
}

int daemon_main()
{
	// Setup server

	int sock_server = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_server < 0) return -1;

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(SERVER_PORT);
	if (bind(sock_server, (struct sockaddr*)&server, sizeof(server)) < 0) return -2;
	listen(sock_server, LISTEN_BACKLOG);

	int sock_client;
	struct sockaddr_in client;
	socklen_t client_len = sizeof(client);
	while (sock_client = accept(sock_server, (struct sockaddr*)&client, &client_len))
	{
		// New incomming connection, create thread
		pthread_t thread;
		if (pthread_create(&thread, NULL, &connection_handler, (void*)sock_client)) return -3;
	}

	if (sock_client < 0) return -4;
	return 0;

	// FIXME: threads are not joined, socket not closed
}

int daemonize()
{
	pid_t pid = fork();
	if (pid < 0) return -1;  // fork error
	if (pid > 0) return 0;   // parent process

	// Daemon process
	pid_t sid = setsid();
	if (sid < 0) return -2;

	chdir("/");
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	int ret = daemon_main();
	return ret == 0 ? 0 : ret - 2;
}

int main(int argc, char *argv[])
{
	return daemonize();
}
