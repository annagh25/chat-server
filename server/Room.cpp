#include "Room.h"
#include "Session.h"

void Room::Join(const std::shared_ptr<Session>& session) {
    {
        std::lock_guard lock(mutex_);
        sessions_[session->Id()] = session;
    }                                            // unlock before Broadcast
    Broadcast("* client " + std::to_string(session->Id()) + " joined\n", session->Id());
}

void Room::Leave(const std::shared_ptr<Session>& session) {
    {
        std::lock_guard lock(mutex_);
        if (sessions_.erase(session->Id()) == 0) {
            return;                              // already left (e.g. read and write both failed)
        }
    }
    Broadcast("* client " + std::to_string(session->Id()) + " left\n", session->Id());
}

void Room::Broadcast(const std::string& message, int sender_id) {
    for (const auto& session : Snapshot()) {     // Deliver runs without the lock held
        if (session->Id() != sender_id) {
            session->Deliver(message);
        }
    }
}

std::vector<std::shared_ptr<Session>> Room::Snapshot() {
    std::lock_guard lock(mutex_);
    std::vector<std::shared_ptr<Session>> result;
    result.reserve(sessions_.size());
    for (const auto& [id, session] : sessions_) {
        result.push_back(session);
    }
    return result;
}
