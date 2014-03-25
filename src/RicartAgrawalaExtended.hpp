#ifndef DISTRIBUTED_LOCKING_RICARD_AGRAWALA_EXTENDED_HPP
#define DISTRIBUTED_LOCKING_RICARD_AGRAWALA_EXTENDED_HPP

#include "RicartAgrawala.hpp"
#include "Agent.hpp"
#include <fipa_acl/fipa_acl.h>

#include <list>
#include <map>

namespace fipa {
namespace distributed_locking {
/**
 * Extension of the Ricart Agrawala algorithm. PROBE->SUCCESS messages have been added, to check if agents are alive.
 */
class RicartAgrawalaExtended : public RicartAgrawala
{
public:
    /**
     * The implemented protocol
     */
    static const protocol::Protocol protocol;
    
    /**
     * Default constructor
     */
    RicartAgrawalaExtended();
    /**
     * Constructor
     */
    RicartAgrawalaExtended(const Agent& self, const std::vector<std::string>& resources);
};
} // namespace distributed_locking
} // namespace fipa

#endif // DISTRIBUTED_LOCKING_RICARD_AGRAWALA_EXTENDED_HPP