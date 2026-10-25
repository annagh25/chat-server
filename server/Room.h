#ifndef RADIO_ROOM_H
#define RADIO_ROOM_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Session;   // forward declaration: Room.h must not include Session.h (include cycle)

class Room {
public:
    void Join(const std::shared_ptr<Session>& session);
    void Leave(const std::shared_ptr<Session>& session);
    void Broadcast(const std::string& message, int sender_id);   // to everyone except sender_id

private:
    std::vector<std::shared_ptr<Session>> Snapshot();   // copy of the sessions, taken under the lock

    std::mutex mutex_;                                   // guards sessions_
    std::map<int, std::shared_ptr<Session>> sessions_;   // id -> session; Room owns the sessions
};

#endif //RADIO_ROOM_H
