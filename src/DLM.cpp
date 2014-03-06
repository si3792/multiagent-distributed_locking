/*
 * Copyright 2014 Satia <email>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "DLM.hpp"

namespace fipa {
namespace distributed_locking {
  
  DLM::DLM()
  {
  }

  DLM::DLM(Agent self, std::vector<Agent> agents)
    : self(self)
    , agents(agents)
  {
  }

  void DLM::addAgent(Agent agent)
  {
    agents.push_back(agent);
  }

  void DLM::setSelf(Agent self)
  {
    this->self = self;
  }

  std::vector<fipa::acl::ACLMessage>& DLM::getOutgoingMessages()
  {
    return outgoingMessages;
  }
  
  
  
} // namespace distributed_locking
} // namespace fipa