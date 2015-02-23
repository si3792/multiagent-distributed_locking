#ifndef DISTRIBUTED_LOCKING_AGENTID_SERIALIZATION_HPP
#define DISTRIBUTED_LOCKING_AGENTID_SERIALIZATION_HPP

#include <fipa_acl/fipa_acl.h>
#include <boost/serialization/split_free.hpp>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/deque.hpp>

namespace boost {
namespace serialization {

/**
 * Support for serialization/deserialization of an fipa::acl::AgentID
 \verbatim
        using namespace fipa::acl;

        AgentID agentIn("agent-name");

        std::stringstream ss;

        // serialize in stringstream
        boost::archive::text_oarchive oa(ss);
        oa << agentIn;

        AgentID agentOut;
        // deserialize
        boost::archive::text_iarchive ia(ss);
        // read state from archive
        ia >> agentOut;
        ...
 \endverbatim
 */
template<class Archive>
inline void save(Archive & ar, const fipa::acl::AgentID& agent, const unsigned int version)
{
    ar << agent.getName();
}

template<class Archive>
inline void load(Archive & ar, fipa::acl::AgentID& agent, const unsigned int version)
{
    std::string name;
    ar >> name;
    agent.setName(name);
}

template<class Archive>
inline void serialize(Archive & ar, fipa::acl::AgentID& agent, const unsigned int file_version)
{
    boost::serialization::split_free(ar, agent, file_version);
}

} // end namespace serialization
} // end boost

#endif // DISTRIBUTED_LOCKING_AGENTID_SERIALIZATION_HPP
