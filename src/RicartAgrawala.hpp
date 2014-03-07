#ifndef DISTRIBUTED_LOCKING_RICARD_AGRAWALA_HPP
#define DISTRIBUTED_LOCKING_RICARD_AGRAWALA_HPP

#include "DLM.hpp"
#include "Agent.hpp"
#include <fipa_acl/fipa_acl.h>

#include <vector>
#include <list>
#include <map>


namespace fipa {
namespace distributed_locking {
/**
 * Implementation of the Ricart Agrawala algorithm. For more information, see http://en.wikipedia.org/wiki/Ricart-Agrawala_algorithm
 */
class RicartAgrawala : public DLM
{
public:
    /**
     * Default constructor
     */
    RicartAgrawala();
    /**
     * Constructor
     */
    RicartAgrawala(const Agent& self, const std::vector<Agent>& agents);

    /**
     * Tries to lock a resource. Subsequently, isLocked() must be called to check the status.
     */
    virtual void lock(const std::string& resource);
    /**
     * Unlocks a resource, that must have been locked before
     */
    virtual void unlock(const std::string& resource);
    /**
     * Checks if the lock for a given resource is held
     */
    virtual bool isLocked(const std::string& resource);
    /**
     * This message is triggered by the wrapping Orogen task, if a message is received
     */
    virtual void onIncomingMessage(const fipa::acl::ACLMessage& message);

private:
    // Messages to be sent later, by leaving the associated critical resource
    std::list<fipa::acl::ACLMessage> mDeferredMessages;
    // Current number for conversation IDs
    int mConversationIDnum;
    // All current interests mapped to the time where the message request was created
    std::map<std::string, base::Time> mInterests;
    // The number of responses for each resource. If it reaches the number of agents, we can enter the critical resource
    std::map<std::string, unsigned int> mNumberOfResponses;
    // All critical resources held at the moment
    std::list<std::string> mHeldResources;

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
    void extractInformation(const fipa::acl::ACLMessage& message, base::Time& time, std::string& resource);
    /**
     * Sends all deferred messages for a certain resource by putting them into outgoingMessages
     */
    void sendAllDeferredMessages(const std::string& resource);
};
} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_RICARD_AGRAWALA_HPP