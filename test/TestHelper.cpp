#include <distributed_locking/DLM.hpp>
#include <base/Logging.hpp>

using namespace fipa;
using namespace fipa::distributed_locking;
using namespace fipa::acl;

/**
 * Forwards all messages that currently await.
 */
void forwardAllMessages(std::list<DLM::Ptr> dlms)
{
    for(std::list<DLM::Ptr>::const_iterator it = dlms.begin(); it != dlms.end(); it++)
    {
        AgentID send = (*it)->getSelf();

        while((*it)->hasOutgoingMessages())
        {
            ACLMessage msg = (*it)->popNextOutgoingMessage();

            for(std::list<DLM::Ptr>::const_iterator it2 = dlms.begin(); it2 != dlms.end(); it2++)
            {
                AgentID receiver = (*it2)->getSelf();
                AgentIDList receivers = msg.getAllReceivers();
                AgentIDList::const_iterator cit = std::find(receivers.begin(), receivers.end(), receiver);

                if(cit != receivers.end())
                {
                    LOG_DEBUG_S << "'" << (*it)->getSelf().getName() << "' --> " << "'" << (*it2)->getSelf().getName();
                    LOG_DEBUG_S << msg.toString();
                    (*it2)->onIncomingMessage(msg);
                }
            }
        }
    }
}
