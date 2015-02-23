#include "RicartAgrawalaExtended.hpp"

using namespace fipa::acl;

namespace fipa {
namespace distributed_locking {

// Set the protocol
const protocol::Protocol RicartAgrawalaExtended::protocol = protocol::RICART_AGRAWALA_EXTENDED;

RicartAgrawalaExtended::RicartAgrawalaExtended()
    : RicartAgrawala()
{
}

RicartAgrawalaExtended::RicartAgrawalaExtended(const fipa::acl::AgentID& self, const std::vector< std::string >& resources)
    : RicartAgrawala(self, resources)
{
}

void RicartAgrawalaExtended::lock(const std::string& resource, const std::list<AgentID>& agents)
{
    fipa::distributed_locking::RicartAgrawala::lock(resource, agents);
    // Start sending probes for all communication partners
    for(std::list<AgentID>::const_iterator it = agents.begin(); it != agents.end(); ++it)
    {
        startRequestingProbes(*it, resource);
    }
}

void RicartAgrawalaExtended::addRespondedAgent(const AgentID& agentName, const std::string& resource)
{
    fipa::distributed_locking::RicartAgrawala::addRespondedAgent(agentName, resource);
    // Stop sending him probes
    stopRequestingProbes(agentName, resource);
}

} // namespace distributed_locking
} // namespace fipa
