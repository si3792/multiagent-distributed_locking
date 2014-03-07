// FIXME: license LGPL
#ifndef DISTRIBUTED_LOCKING_DLM_HPP
#define DISTRIBUTED_LOCKING_DLM_HPP

#include <vector>
#include <list>
#include <stdexcept>

#include <fipa_acl/fipa_acl.h>
#include <distributed_locking/Agent.hpp>

/** \mainpage Distributed Locking Mechanism
 *  This library provides an interface for a locking mechanism on distributed systems. This interface is given by the abstract class DLM.
 *
 * Currently, the Ricart Agrawala algorithm ( http://en.wikipedia.org/wiki/Ricart-Agrawala_algorithm ) is the only implementation of that interface.
 */

namespace fipa {
namespace distributed_locking {
/**
 * A distributed locking mechanism. This class is abstract.
 */
class DLM
{
public:
    /**
     * Default constructor
     */
    DLM();
    /**
     * Constructor
     */
    DLM(const Agent& self, const std::vector<Agent>& agents);

    /**
     * Adds an agent to the list. Must not be called more than once with the same agent.
     */
    void addAgent(Agent agent);

    /**
     * Sets the agent this DLM works with.
     */
    void setSelf(Agent self);

    /**
     * Gets the next outgoing message, and removes it from the internal queue. Used by the higher instance that uses this library.
     */
    fipa::acl::ACLMessage& popNextOutgoingMessage();

    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource)
    {
        throw std::runtime_error("DLM::lock not implemented");
    }
    /**
     * Unlocks a resource, that should have been locked before.
     */
    virtual void unlock(const std::string& resource)
    {
        throw std::runtime_error("DLM::unlock not implemented");
    }
    /**
     * Checks if the lock for a given resource is held.
     */
    virtual bool isLocked(const std::string& resource)
    {
        throw std::runtime_error("DLM::isLocked not implemented");
    }

    /**
     * This message is triggered by higher instance that uses this library, if a message is received. Sequential calls must be guaranteed.
     */
    virtual void onIncomingMessage(const fipa::acl::ACLMessage& message)
    {
        throw std::runtime_error("DLM::onIncomingMessage not implemented");
    }


protected:
    // The agent represented by this DLM
    Agent mSelf;
    // The agents to communicate with, excluding self
    std::vector<Agent> mAgents;

    // List of outgoing messages. The Orogen task checks in intervals and forwards the messages.
    std::list<fipa::acl::ACLMessage> mOutgoingMessages;
};

} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_DLM_HPP