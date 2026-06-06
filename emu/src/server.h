/* server.h — WebSocket server + lobby manager entry point.
 *
 * server_main() runs the parent process: an HTTP/WebSocket server (mongoose)
 * that manages lobbies. Each lobby is one forked child emulator process with its
 * own g_emu + Unicorn, connected to the parent over a socketpair. The parent
 * never creates Unicorn; the child does, after fork() (fork-safety). */
#ifndef YVIO_SERVER_H
#define YVIO_SERVER_H

int server_main(const char *root, const char *lang, int port);

#endif /* YVIO_SERVER_H */
