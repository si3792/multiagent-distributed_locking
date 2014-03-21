#include <distributed_locking/DLM.hpp>

using namespace fipa;
using namespace fipa::distributed_locking;
using namespace fipa::acl;

/**
 * Forwards all messages that currently await.
 */
void forwardAllMessages(std::list<DLM*> dlms)
{
    for(std::list<DLM*>::const_iterator it = dlms.begin(); it != dlms.end(); it++)
    {
        while((*it)->hasOutgoingMessages())
        {
            ACLMessage msg = (*it)->popNextOutgoingMessage();
            for(std::list<DLM*>::const_iterator it2 = dlms.begin(); it2 != dlms.end(); it2++)
            {
                (*it2)->onIncomingMessage(msg);
            }
        }
    }
}