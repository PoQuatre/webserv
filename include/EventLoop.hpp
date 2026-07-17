/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:07:46 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/17 19:41:28 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <map>
#include <vector>

#include "Connection.hpp"
#include "Server.hpp"

class EventLoop {
public:
    explicit EventLoop(std::vector<Server> &servers);
    ~EventLoop();

    bool run();

private:
    EventLoop(const EventLoop &other);
    EventLoop &operator=(const EventLoop &other);

    bool setup();
    bool create_epoll();
    bool register_servers();
    bool init_signal_handlers();
    void cleanup();

    struct EventSource {
        enum Type { SOURCE_LISTENER, SOURCE_SIGNAL, SOURCE_CLIENT };

        EventSource();
        explicit EventSource(const Server &server_context);
        explicit EventSource(Connection &connection_context);

        Type type;
        const Server *server;
        Connection *connection;
    };

    bool add_source(int32_t fd, uint32_t events, const EventSource &source);
    bool update_source_events(int32_t fd, uint32_t events) const;
    void remove_source(int32_t fd);

    void process_io_event(int32_t fd, uint32_t events, bool &running);
    void dispatch_source(
        int32_t fd, uint32_t events, const EventSource &source, bool &running);
    void accept_client(int32_t sockfd, const Server &server);
    void process_client(int32_t fd, uint32_t events, Connection &conn);
    void dispatch_pending(int32_t fd, uint32_t events, Connection &conn) const;
    void close_client(int32_t clientfd, Connection &conn);

    std::vector<Server> &_servers;
    std::map<int32_t, EventSource> _sources;
    std::map<int32_t, Connection> _connections;
    int32_t _epollfd;
};
