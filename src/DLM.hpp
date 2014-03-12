#ifndef DISTRIBUTED_LOCKING_DLM_HPP
#define DISTRIBUTED_LOCKING_DLM_HPP

#include "Agent.hpp"
#include <fipa_acl/fipa_acl.h>

#include <vector>
#include <list>
#include <stdexcept>
#include <boost/assign/list_of.hpp>


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
        \enum LockState
        \brief an enum of all the possible lock states per resource
    */
    enum LockState { NOT_INTERESTED = 0, INTERESTED, LOCKED };
    
    /**
        \enum Protocol
        \brief an enum of all the implementations
    */
    enum Protocol { RICART_AGRAWALA = 0 };
    
    /**
     * Default constructor
     */
    DLM();
    /**
     * Constructor
     */
    DLM(const Agent& self);

    /**
     * Sets the agent this DLM works with.
     */
    void setSelf(const Agent& self);

    /**
     * Gets the next outgoing message, and removes it from the internal queue. Used by the higher instance that uses this library.
     * Must only be called if hasOutgoingMessages == true.
     */
    fipa::acl::ACLMessage popNextOutgoingMessage();
    /**
     * True, if there are outgoing messages than can be obtained with popNextOutgoingMessage.
     */
    bool hasOutgoingMessages();

    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource, const std::list<Agent>& agents)
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
     * Gets the lock state for a resource.
     */
    virtual LockState getLockState(const std::string& resource)
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

    // List of outgoing messages.
    std::list<fipa::acl::ACLMessage> mOutgoingMessages;
    
    // TODO now everyone must import boost :/
    //static std::map<DLM::Protocol, std::string> protocolTxt = boost::assign::map_list_of
      //  (DLM::RICART_AGRAWALA, "ricart_agrawala");
};

} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_DLM_HPP