Instructions for manually processing one round of algorand protocol

# Steps

Description of steps required in one round of algorand protocol

  1. accounts check whether could be leader
    - hash( account signature( round, 1, random value ) ) <= some probability
  2. potential leaders prepare next block
  3. accounts check whether could be verifier
    - hash( account signature( round, substep, random value ) ) >= some other probability
  4. verifiers select leader (todo: by what time?)
  5. next set of verifiers select next block (todo: by what time?)
