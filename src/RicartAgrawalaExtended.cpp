#include "RicartAgrawalaExtended.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <stdexcept>

namespace fipa {
namespace distributed_locking {

// Set the protocol
const protocol::Protocol RicartAgrawalaExtended::protocol = protocol::RICART_AGRAWALA_EXTENDED;
    
RicartAgrawalaExtended::RicartAgrawalaExtended() 
    : RicartAgrawala()
{
}

RicartAgrawalaExtended::RicartAgrawalaExtended(const fipa::Agent& self, const std::vector< std::string >& resources) 
    : RicartAgrawala(self, resources)
{
}

void RicartAgrawalaExtended::lock(const std::string& resource, const std::list< Agent >& agents)
{
    fipa::distributed_locking::RicartAgrawala::lock(resource, agents);
    // Start sending probes for all communication partners
    for(std::list<Agent>::const_iterator it = agents.begin(); it != agents.end(); it++)
    {
        startRequestingProbes(it->identifier, resource);
    }
}

void RicartAgrawalaExtended::addRespondedAgent(std::string agentName, std::string resource)
{
    fipa::distributed_locking::RicartAgrawala::addRespondedAgent(agentName, resource);
    // Stop sending him probes
    stopRequestingProbes(agentName, resource);
}

} // namespace distributed_locking
} // namespace fipa