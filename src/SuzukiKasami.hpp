#ifndef DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP
#define DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP

#include <queue>
#include <fipa_acl/fipa_acl.h>
#include <distributed_locking/DLM.hpp>
#include <distributed_locking/AgentIDSerialization.hpp>

namespace fipa {
namespace distributed_locking {
/**
 * Implementation of the Suzuki Kasami algorithm. For more information, see http://en.wikipedia.org/wiki/Suzuki-Kasami_algorithm
 */
class SuzukiKasami : public DLM
{
    friend class ResourceLockState;
public:
    /**
     * The token used in this protocol.
     */
    struct Token
    {
        static std::string getTypeName() { return "suzuki_kasami::Token"; }

        // LastRequestNumber for each of the agents
        std::map<fipa::acl::AgentID, int> mLastRequestNumber;

         // Queue of agents waiting for the token
        std::deque<fipa::acl::AgentID> mQueue;

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
     * Constructor
     */
    SuzukiKasami(const fipa::acl::AgentID& self, const std::vector<std::string>& resources);

    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource, const fipa::acl::AgentIDList& agents);
    /**
     * Unlocks a resource, that must have been locked before
     */
    virtual void unlock(const std::string& resource);
    /**
     * Gets the lock state for a resource.
     */
    virtual lock_state::LockState getLockState(const std::string& resource) const;
    /**
     * This message is triggered by the higher instance that uses this library, if a message is received. Sequential calls must be guaranteed.
     * Subclasses MUST call the base implementation, to handle default messages
     */
    virtual bool onIncomingMessage(const fipa::acl::ACLMessage& message);
    /**
     * This message is called by the DLM, if an agent does not respond a REQUEST messages with CONFIRM after a certain timeout.
     * Subclasses can and should react according to the algorithm.
     */
    virtual void agentFailed(const fipa::acl::AgentID& agentName);

protected:
    
    /**
     * Nested class representing an inner state for a certain resource.
     * It is mapped to its resource name.
     */
    struct ResourceLockState
    {
        // The token.
        Token mToken;
        // Whether the token is currently held
        bool mHoldingToken;
        // Everyone to inform when locking
        fipa::acl::AgentIDList mCommunicationPartners;
        // Last known request number for each of the agents
        std::map<fipa::acl::AgentID, int> mRequestNumber;
        // The lock state, initially not interested (=0)
        lock_state::LockState mState;
        // The requestor mapped to the conversationID, which is relevant if we're interested and get a failure message back
        std::map<fipa::acl::AgentID, std::string> mConversationID;

        void removeCommunicationPartner(const fipa::acl::AgentID& agent);
    };
    
    // All resources mapped to the their ResourceLockStates
    std::map<std::string, ResourceLockState> mLockStates;

    /**
     * Handles an incoming request for the token
     */
    void handleIncomingTokenRequest(const fipa::acl::ACLMessage& message);

    /**
     * Handles an incoming token
     */
    virtual void handleIncomingToken(const fipa::acl::ACLMessage& message);

    /**
     * Handles an incoming failure
     */
    void handleIncomingFailure(const fipa::acl::ACLMessage& message);

    /**
     * Actually handles an incoming failure.
     */
    void handleIncomingFailure(const std::string& resource, const fipa::acl::AgentID& intendedReceiver);

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
    virtual void sendToken(const fipa::acl::AgentID& receiver, const std::string& resource);

    /**
     * Request the token
     */
    void requestToken(const std::string& resource, const fipa::acl::AgentIDList& agents);

    /**
     * Update the token based on an incoming request
     */
    void updateToken(const std::string& resource, const fipa::acl::AgentID& requestor, int sequenceNumber);

    /**
     * Forwards the token to the next person in the queue.
     */
    virtual void forwardToken(const std::string& resource);
    /**
     * Will always return false, as the original SuzukiKasami algorithm cannot keep track of the token owners.
     */
    virtual bool isTokenHolder(const std::string& resource, const fipa::acl::AgentID& agent);


    bool hasOutstandingRequest(const std::string& resource, const fipa::acl::AgentID& agent);
    
};
} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_SUZUKI_KASAMI_HPP
