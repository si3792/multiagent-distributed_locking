#ifndef DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP
#define DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP

#include "DLM.hpp"
#include "Agent.hpp"
#include <fipa_acl/fipa_acl.h>

#include <deque>
#include <list>
#include <map>


namespace fipa {
namespace distributed_locking {
/**
 * Implementation of the Suzuki Kasami algorithm. For more information, see http://en.wikipedia.org/wiki/Suzuki-Kasami_algorithm
 */
class SuzukiKasami : public DLM
{
public:
    /**
     * The implemented protocol
     */
    static const protocol::Protocol protocol;
    
    /**
     * Default constructor
     */
    SuzukiKasami();
    /**
     * Constructor
     */
    SuzukiKasami(const Agent& self);

    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource, const std::list<Agent>& agents);
    /**
     * Unlocks a resource, that must have been locked before
     */
    virtual void unlock(const std::string& resource);
    /**
     * Gets the lock state for a resource.
     */
    virtual lock_state::LockState getLockState(const std::string& resource);
    /**
     * This message is triggered by the wrapping Orogen task, if a message is received
     */
    virtual void onIncomingMessage(const fipa::acl::ACLMessage& message);

private:
    /**
     * A token used in this protocol
     */
    struct Token
    {
        // LastRequestNumber for each of the agents
        std::map<std::string, int> mLastRequestNumber;
         // Queue of agents waiting for the token
        std::deque<std::string> mQueue;
        
        /**
         * Boost serialization method
         */
        template<class Archive>
        void serialize(Archive& ar, unsigned int version)
        {
            ar & mLastRequestNumber;
            ar & mQueue;
        }
    };
    
    /**
     * Nested class representing an inner state for a certain resource.
     * It is mapped to its resource name.
     */
    struct ResourceLockState
    {
        // The token.
        Token mToken; // FIXME when are tokens created!?
        // Whether the token is currently held
        bool mHoldingToken;
        // Everyone to inform when locking
        std::list<Agent> mCommunicationPartners;
        // Last known request number for each of the agents
        std::map<std::string, int> mRequestNumber;
        // The lock state, initially not interested (=0)
        lock_state::LockState mState;
    };
    
    // Current number for conversation IDs
    int mConversationIDnum;
    // All resources mapped to the their ResourceLockStates
    std::map<std::string, ResourceLockState> mLockStates;

    /**
     * Handles an incoming request
     */
    void handleIncomingRequest(const fipa::acl::ACLMessage& message);
    /**
     * Handles an incoming response
     */
    void handleIncomingResponse(const fipa::acl::ACLMessage& message);
    /**
     * Extracts the information from the content and saves it in the passed references
     */
    void extractInformation(const fipa::acl::ACLMessage& message, std::string& resource, int& sequence_number);
    /**
     * Extracts the information from the content and saves it in the passed references
     */
    void extractInformation(const fipa::acl::ACLMessage& message, std::string& resource, Token& token);
    /**
     * Send the token to the receiver. No checks (token held, lock not held) are made!
     */
    void sendToken(const std::string& receiver, const std::string& resource);
    
};
} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP