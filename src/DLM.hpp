#ifndef DISTRIBUTED_LOCKING_DLM_HPP
#define DISTRIBUTED_LOCKING_DLM_HPP

#include "Agent.hpp"
#include <fipa_acl/fipa_acl.h>

#include <list>


/** \mainpage Distributed Locking Mechanism
 *  This library provides an interface for a locking mechanism on distributed systems. This interface is given by the abstract class DLM.
 *
 * Currently, the Ricart Agrawala algorithm ( http://en.wikipedia.org/wiki/Ricart-Agrawala_algorithm )
 * and the Suzuki Kasami algorithm ( http://en.wikipedia.org/wiki/Suzuki-Kasami_algorithm ) are implemented.
 * 
 * \section Code
 * The following code snippet shows the basic usage of this library. This is relevant implementing
 * wrapping tasks, e.g., using Orogen. The task has to use the methods seen below and take care of the
 * ACL message transportation.
 * 
 \verbatim
    #include <distributed_locking/DLM.hpp>

    using namespace fipa::distributed_locking;
    
    // Create an agent
    fipa::Agent agent ("agent1");
    // Create the DLM with the desired protocol using the factory method.
    DLM* dlm = DLM::dlmFactory(protocol::RICART_AGRAWALA, a1);
    
    ...    
    // Get outgoing messages:
    if(dlm->hasOutgoingMessages())
        ACLMessage msg = dlm->popNextOutgoingMessage();
    
    ...    
    // Forward incoming messages
    dlm->onIncomingMessage(otherMsg);
    
    ...
    // Lock with a list of agents (using boost::assign::list_of)
    dlm1->lock("resource_name", boost::assign::list_of(agent2)(agent3));
    ...
    // Check lock status
    if(dlm->getLockState("resource_name") == lock_state::LOCKED) // Can be LOCKED, INTERESTED, or NOT_INTERESTED
    {
        // We are in the criticial section
    }
    ...
    // Unlock
    dlm->unlock("resource_name");
    
 \endverbatim
 * 
 */

namespace fipa {
namespace distributed_locking {

namespace lock_state {
/**
    \enum LockState
    \brief an enum of all the possible lock states per resource
*/
enum LockState { NOT_INTERESTED = 0, INTERESTED, LOCKED };
} // namespace lock_state

namespace protocol {
/**
    \enum Protocol
    \brief an enum of all the implementations
*/
enum Protocol { RICART_AGRAWALA = 0, SUZUKI_KASAMI,
    // Following values only for enumertaing over this enum
    PROTOCOL_START = RICART_AGRAWALA, PROTOCOL_END = SUZUKI_KASAMI
};    
} // namespace protocol

/**
 * A distributed locking mechanism. This class is abstract.
 */
class DLM
{
public:
    /**
     * Factory method to get a pointer to a certain DLM implementation
     */
    static DLM* dlmFactory(fipa::distributed_locking::protocol::Protocol implementation, const fipa::Agent& self);
    
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
     * Gets the agent this DLM works with.
     */
    const Agent& getSelf();

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
    virtual void lock(const std::string& resource, const std::list<Agent>& agents);
    /**
     * Unlocks a resource, that should have been locked before.
     */
    virtual void unlock(const std::string& resource);
    /**
     * Gets the lock state for a resource.
     */
    virtual lock_state::LockState getLockState(const std::string& resource);

    /**
     * This message is triggered by higher instance that uses this library, if a message is received. Sequential calls must be guaranteed.
     */
    virtual void onIncomingMessage(const fipa::acl::ACLMessage& message);


protected:
    // The agent represented by this DLM
    Agent mSelf;

    // List of outgoing messages.
    std::list<fipa::acl::ACLMessage> mOutgoingMessages;
    
    // A mapping between protocols and strings
    static std::map<protocol::Protocol, std::string> protocolTxt;
};

} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_DLM_HPP