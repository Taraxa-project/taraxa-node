var searchData=
[
  ['dag_4448',['Dag',['../group___d_a_g.html#a6266d656a7eba53bac6fa8dc7e7eb5da',1,'taraxa::Dag::Dag(const Dag &amp;)=default'],['../group___d_a_g.html#a7def37042439fa3c713ea8861d40b199',1,'taraxa::Dag::Dag(blk_hash_t const &amp;dag_genesis_block_hash, addr_t node_addr)'],['../group___d_a_g.html#a91d4f1eb81776e18f3908374d0eef614',1,'taraxa::Dag::Dag(Dag &amp;&amp;)=default']]],
  ['dagblock_4449',['DagBlock',['../classtaraxa_1_1_dag_block.html#a1f73650dd7f598ac35927a145f8a81a3',1,'taraxa::DagBlock::DagBlock(string const &amp;json)'],['../classtaraxa_1_1_dag_block.html#a92d0d1d65b53c557b39f06aaae6f9b47',1,'taraxa::DagBlock::DagBlock(dev::bytes const &amp;_rlp)'],['../classtaraxa_1_1_dag_block.html#afb5a52db03e70fafdb1d35d56e972806',1,'taraxa::DagBlock::DagBlock(dev::RLP const &amp;_rlp)'],['../classtaraxa_1_1_dag_block.html#ad780ba7392776f8e2c1c8b80c414cc82',1,'taraxa::DagBlock::DagBlock(Json::Value const &amp;doc)'],['../classtaraxa_1_1_dag_block.html#a5918ea1955486417267213c3e9a12a74',1,'taraxa::DagBlock::DagBlock(blk_hash_t const &amp;pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, uint64_t est, VdfSortition vdf, secret_t const &amp;sk)'],['../classtaraxa_1_1_dag_block.html#a00e51bfd48cfa3dab7357938cb4e65b0',1,'taraxa::DagBlock::DagBlock(blk_hash_t const &amp;pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, secret_t const &amp;sk)'],['../classtaraxa_1_1_dag_block.html#acfc470101e8d0dcdc4e1ed2f7e3f755e',1,'taraxa::DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, sig_t signature, blk_hash_t hash, addr_t sender)'],['../classtaraxa_1_1_dag_block.html#ad178f6fc15e23d4486dbfcaa5dc12136',1,'taraxa::DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, uint64_t est, sig_t signature, blk_hash_t hash, addr_t sender)'],['../classtaraxa_1_1_dag_block.html#a14040cdc9db1c166b9c0d021ce9bf15d',1,'taraxa::DagBlock::DagBlock()=default'],['../classgraphql_1_1taraxa_1_1_dag_block.html#adca4dbb4250b88dbfc86d405be7060b9',1,'graphql::taraxa::DagBlock::DagBlock()']]],
  ['dagblockindb_4450',['dagBlockInDb',['../classtaraxa_1_1_db_storage.html#a0e7339685ce0f30cb9186ccec58709fa',1,'taraxa::DbStorage']]],
  ['dagblockpackethandler_4451',['DagBlockPacketHandler',['../classtaraxa_1_1network_1_1tarcap_1_1_dag_block_packet_handler.html#af498b7dee8530ddfebc2af5da181bc01',1,'taraxa::network::tarcap::DagBlockPacketHandler']]],
  ['dagblockproposer_4452',['DagBlockProposer',['../group___d_a_g.html#ade71bd4eb9aa6db990841e3173e4fa03',1,'taraxa::DagBlockProposer::DagBlockProposer(const DagBlockProposerConfig &amp;bp_config, std::shared_ptr&lt; DagManager &gt; dag_mgr, std::shared_ptr&lt; TransactionManager &gt; trx_mgr, std::shared_ptr&lt; final_chain::FinalChain &gt; final_chain, std::shared_ptr&lt; DbStorage &gt; db, std::shared_ptr&lt; KeyManager &gt; key_manager, addr_t node_addr, secret_t node_sk, vrf_wrapper::vrf_sk_t vrf_sk, uint64_t pbft_gas_limit, uint64_t dag_gas_limit)'],['../group___d_a_g.html#aace097ada959a377e46c6a6e57c43c21',1,'taraxa::DagBlockProposer::DagBlockProposer(const DagBlockProposer &amp;)=delete'],['../group___d_a_g.html#ab6d80eccedea603e03d4a194c5df0327',1,'taraxa::DagBlockProposer::DagBlockProposer(DagBlockProposer &amp;&amp;)=delete']]],
  ['dagfrontier_4453',['DagFrontier',['../structtaraxa_1_1_dag_frontier.html#ab0af91682b5f811569387e80042179d6',1,'taraxa::DagFrontier::DagFrontier()=default'],['../structtaraxa_1_1_dag_frontier.html#a5956303317c4cd51a01c79919a38b20c',1,'taraxa::DagFrontier::DagFrontier(blk_hash_t const &amp;pivot, vec_blk_t const &amp;tips)']]],
  ['dagmanager_4454',['DagManager',['../group___d_a_g.html#ac9aed798e9b30f18abf9dc910adeb991',1,'taraxa::DagManager::DagManager(const DagBlock &amp;dag_genesis_block, addr_t node_addr, const SortitionConfig &amp;sortition_config, const DagConfig &amp;dag_config, std::shared_ptr&lt; TransactionManager &gt; trx_mgr, std::shared_ptr&lt; PbftChain &gt; pbft_chain, std::shared_ptr&lt; FinalChain &gt; final_chain, std::shared_ptr&lt; DbStorage &gt; db, std::shared_ptr&lt; KeyManager &gt; key_manager, uint64_t pbft_gas_limit, bool is_light_node=false, uint64_t light_node_history=0, uint32_t max_levels_per_period=kMaxLevelsPerPeriod, uint32_t dag_expiry_limit=kDagExpiryLevelLimit)'],['../group___d_a_g.html#aa0fe82cc22a187cab97099ef465bed02',1,'taraxa::DagManager::DagManager(const DagManager &amp;)=delete'],['../group___d_a_g.html#a1a957529304d407e997ca611b4530ab7',1,'taraxa::DagManager::DagManager(DagManager &amp;&amp;)=delete']]],
  ['dagsyncingallowed_4455',['dagSyncingAllowed',['../classtaraxa_1_1network_1_1tarcap_1_1_taraxa_peer.html#a2ca9c17057d092453dd4e0337a27bafe',1,'taraxa::network::tarcap::TaraxaPeer']]],
  ['dagsyncpackethandler_4456',['DagSyncPacketHandler',['../classtaraxa_1_1network_1_1tarcap_1_1_dag_sync_packet_handler.html#a53e2607d78ce2f5ff50fdd1b93355336',1,'taraxa::network::tarcap::DagSyncPacketHandler']]],
  ['data_4457',['data',['../classdev_1_1_fixed_hash.html#a49662c17a82b8cdeb1a81d2244c61e13',1,'dev::FixedHash::data()'],['../classdev_1_1_fixed_hash.html#a7aa90cb5b66e28cf93b841af6da80c6a',1,'dev::FixedHash::data() const'],['../classdev_1_1_secure_fixed_hash.html#abc47be5c36cc816ed6b0d6517fe97c3c',1,'dev::SecureFixedHash::data()'],['../classdev_1_1_r_l_p.html#ae1f6d3577e4d896ba845933fa71a4b35',1,'dev::RLP::data()'],['../classdev_1_1vector__ref.html#adadc63c541b917aeb70d3385fad42ec2',1,'dev::vector_ref::data()'],['../classdev_1_1p2p_1_1_r_l_p_x_packet.html#a42dfeb2ea6d5911070d732806697f9a4',1,'dev::p2p::RLPXPacket::data()']]],
  ['dbexception_4458',['DbException',['../classtaraxa_1_1_db_exception.html#a7686db339de4857e8acf8a7fa8994751',1,'taraxa::DbException::DbException(const std::string &amp;desc)'],['../classtaraxa_1_1_db_exception.html#a855ce1390542835d262d3a67a1bea980',1,'taraxa::DbException::DbException(const DbException &amp;)=default'],['../classtaraxa_1_1_db_exception.html#a9dc5cb437b424f6d423d47d173d8165a',1,'taraxa::DbException::DbException(DbException &amp;&amp;)=default']]],
  ['dbstorage_4459',['DbStorage',['../classtaraxa_1_1_db_storage.html#a79a9960113464b742cc432cd57415486',1,'taraxa::DbStorage::DbStorage(DbStorage &amp;&amp;)=delete'],['../classtaraxa_1_1_db_storage.html#aa41cccbaab43c81d7297013102e8f83c',1,'taraxa::DbStorage::DbStorage(fs::path const &amp;base_path, uint32_t db_snapshot_each_n_pbft_block=0, uint32_t max_open_files=0, uint32_t db_max_snapshots=0, PbftPeriod db_revert_to_period=0, addr_t node_addr=addr_t(), bool rebuild=false)'],['../classtaraxa_1_1_db_storage.html#af1708845f17561a106abe94391a964f7',1,'taraxa::DbStorage::DbStorage(const DbStorage &amp;)=delete']]],
  ['dbstoragepath_4460',['dbStoragePath',['../classtaraxa_1_1_db_storage.html#a9b3cbfc1c55197bafc743ffc9f886ab5',1,'taraxa::DbStorage']]],
  ['dbversion_4461',['dbVersion',['../classtaraxa_1_1storage_1_1migration_1_1_base.html#a0d297d52410384d72e8d7283abbdaf5e',1,'taraxa::storage::migration::Base::dbVersion()'],['../classtaraxa_1_1storage_1_1migration_1_1_transaction_hashes.html#a411df98f12f7cf0b1949a5d18548f165',1,'taraxa::storage::migration::TransactionHashes::dbVersion()']]],
  ['debug_4462',['Debug',['../classtaraxa_1_1net_1_1_debug.html#ae52c9ee7c3fc2c69af9bf6dd293629a8',1,'taraxa::net::Debug']]],
  ['debug_5ftracecall_4463',['debug_traceCall',['../classtaraxa_1_1net_1_1_debug.html#a85f1d4335d4676a3735967ad0d2cfbea',1,'taraxa::net::Debug::debug_traceCall()'],['../classtaraxa_1_1net_1_1_debug_client.html#aaea8a1290c23a206d32a975a12d92bc5',1,'taraxa::net::DebugClient::debug_traceCall()'],['../classtaraxa_1_1net_1_1_debug_face.html#a7ddab247275cc6a4272b888989bcb8ab',1,'taraxa::net::DebugFace::debug_traceCall(const Json::Value &amp;param1, const std::string &amp;param2)=0']]],
  ['debug_5ftracecalli_4464',['debug_traceCallI',['../classtaraxa_1_1net_1_1_debug_face.html#a5cf8ba06fbaf6bba56c7df8778546db0',1,'taraxa::net::DebugFace']]],
  ['debug_5ftracetransaction_4465',['debug_traceTransaction',['../classtaraxa_1_1net_1_1_debug.html#a3daf730fcb91d8266c2eb4f327741289',1,'taraxa::net::Debug::debug_traceTransaction()'],['../classtaraxa_1_1net_1_1_debug_client.html#aa50bd584eff17885f5b20ac3534dcf15',1,'taraxa::net::DebugClient::debug_traceTransaction()'],['../classtaraxa_1_1net_1_1_debug_face.html#a1546ee4d5b1a0dd831696902b4a0ba95',1,'taraxa::net::DebugFace::debug_traceTransaction(const std::string &amp;param1)=0']]],
  ['debug_5ftracetransactioni_4466',['debug_traceTransactionI',['../classtaraxa_1_1net_1_1_debug_face.html#ad2128a5ef7de3b36ed5e34031b6b965c',1,'taraxa::net::DebugFace']]],
  ['debugclient_4467',['DebugClient',['../classtaraxa_1_1net_1_1_debug_client.html#a0176a459e0cda8993a9150875c416174',1,'taraxa::net::DebugClient']]],
  ['debugface_4468',['DebugFace',['../classtaraxa_1_1net_1_1_debug_face.html#ac7c7de8a883f6443df79aa9d5b38506b',1,'taraxa::net::DebugFace']]],
  ['dec_5fddos_5fprotection_5fconfig_5fjson_4469',['dec_ddos_protection_config_json',['../namespacetaraxa.html#aa8f416533f421d87c50990a6aa205f6a',1,'taraxa']]],
  ['dec_5fjson_4470',['dec_json',['../namespacetaraxa.html#a2850298ee300bd104ca9db9352abf63f',1,'taraxa::dec_json(const Json::Value &amp;json, VdfParams &amp;obj)'],['../namespacetaraxa.html#a6ea909654b6cd15b58987ec4d1a9d0ab',1,'taraxa::dec_json(const Json::Value &amp;json, VrfParams &amp;obj)'],['../namespacetaraxa.html#a124545b9fa417446dd410b9c69701744',1,'taraxa::dec_json(const Json::Value &amp;json, SortitionParams &amp;obj)'],['../namespacetaraxa.html#ad2584bf1da77275f513d1378d392fd99',1,'taraxa::dec_json(const Json::Value &amp;json, SortitionConfig &amp;obj)'],['../namespacetaraxa.html#aab796ac6602a0ecde1f58bc181127628',1,'taraxa::dec_json(const Json::Value &amp;json)'],['../namespacetaraxa.html#aad70d4845a45158f0122129ce50737b7',1,'taraxa::dec_json(const Json::Value &amp;json, PrometheusConfig &amp;config)'],['../hardfork_8cpp.html#ab616290c2361c3da2f52efddcf15ba25',1,'dec_json():&#160;hardfork.cpp'],['../namespacetaraxa_1_1state__api.html#a71678cb3f5feb67a738df3d7d2f45270',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, Config &amp;obj)'],['../namespacetaraxa_1_1state__api.html#a8430d4247900167bc1ac842c70484cc3',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, DPOSConfig &amp;obj)'],['../namespacetaraxa.html#a74ad03c0d7a830dd89d7f10d6f941077',1,'taraxa::dec_json()'],['../namespacetaraxa_1_1state__api.html#a88679bd672ad12f8a2517728f1c1109f',1,'taraxa::state_api::dec_json()'],['../namespacetaraxa.html#a4fe5a976d3d25a7348800b50b8d377b5',1,'taraxa::dec_json(Json::Value const &amp;json, DBConfig &amp;db_config)'],['../namespacetaraxa.html#a681e859a51cfd602ec476273ae67ce91',1,'taraxa::dec_json(const Json::Value &amp;json, DagBlockProposerConfig &amp;obj)'],['../namespacetaraxa.html#a9c00e55e957ffb65d8891c88785cf57e',1,'taraxa::dec_json(const Json::Value &amp;json, DagConfig &amp;obj)'],['../namespacetaraxa.html#a1a7add6cdc2ba2969325ea1ad4723859',1,'taraxa::dec_json(Json::Value const &amp;json, GasPriceConfig &amp;obj)'],['../namespacetaraxa.html#a110379ae3c76cbe68346680bc1aab397',1,'taraxa::dec_json(Json::Value const &amp;json, Genesis &amp;obj)'],['../hardfork_8hpp.html#ab616290c2361c3da2f52efddcf15ba25',1,'dec_json():&#160;hardfork.cpp'],['../namespacetaraxa.html#aa5e15ca58e0ad240f75a8c70883a00e0',1,'taraxa::dec_json(const Json::Value &amp;json, NetworkConfig &amp;network)'],['../namespacetaraxa.html#a6d4575c65b00ce4d77c66f41277ce170',1,'taraxa::dec_json(Json::Value const &amp;json, PbftConfig &amp;obj)'],['../namespacetaraxa_1_1state__api.html#a142767c95d8323471aee83d86833ee8c',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, uint64_t chain_id, EVMChainConfig &amp;obj)'],['../namespacetaraxa_1_1state__api.html#a0cdba49b2802bab1d0da7a671714e77b',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, BalanceMap &amp;obj)']]],
  ['decoder_5fcb_5fc_4471',['decoder_cb_c',['../namespacetaraxa_1_1state__api.html#a21f5167bc7fd439ed8b01d89c1bd814e',1,'taraxa::state_api']]],
  ['decompressframe_4472',['decompressFrame',['../classdev_1_1p2p_1_1_r_l_p_x_frame_coder.html#a477edddcaebca0ee2d9a39f9ea6d6c15',1,'dev::p2p::RLPXFrameCoder']]],
  ['decrementactworkerscount_4473',['decrementActWorkersCount',['../classtaraxa_1_1network_1_1tarcap_1_1_packets_queue.html#a6241ef5233ed96c1a75762f5787aae63',1,'taraxa::network::tarcap::PacketsQueue']]],
  ['decrypt_4474',['decrypt',['../classdev_1_1crypto_1_1_secp256k1_p_p.html#ad3358d52e135b75e3e86110c68051c64',1,'dev::crypto::Secp256k1PP::decrypt()'],['../namespacedev.html#a91ee12ea84c5b9166e35e9af56010e8c',1,'dev::decrypt(Secret const &amp;_k, bytesConstRef _cipher, bytes &amp;o_plaintext)']]],
  ['decryptaes128ctr_4475',['decryptAES128CTR',['../namespacedev.html#a68f8f8b4cd62360ce45a1b6ef467ce59',1,'dev']]],
  ['decryptecies_4476',['decryptECIES',['../classdev_1_1crypto_1_1_secp256k1_p_p.html#a8e0bfb8aaa2e339e88a893809eae0a9c',1,'dev::crypto::Secp256k1PP::decryptECIES(Secret const &amp;_k, bytes &amp;io_text)'],['../classdev_1_1crypto_1_1_secp256k1_p_p.html#ac7ff99c05c14afd9354f3c2f841b1865',1,'dev::crypto::Secp256k1PP::decryptECIES(Secret const &amp;_k, bytesConstRef _sharedMacData, bytes &amp;io_text)'],['../namespacedev.html#ae1ddd7b7ee60c0bc8d6e047b4b84d1e4',1,'dev::decryptECIES(Secret const &amp;_k, bytesConstRef _sharedMacData, bytesConstRef _cipher, bytes &amp;o_plaintext)'],['../namespacedev.html#a46bb4787ff142b9cd040af5b15334013',1,'dev::decryptECIES(Secret const &amp;_k, bytesConstRef _cipher, bytes &amp;o_plaintext)']]],
  ['decryptsymnoauth_4477',['decryptSymNoAuth',['../namespacedev.html#a3bef0b6d00f9f9b2b3a558e51cb9d81d',1,'dev::decryptSymNoAuth(SecureFixedHash&lt; 32 &gt; const &amp;_k, h128 const &amp;_iv, bytesConstRef _cipher)'],['../namespacedev.html#a34883d4634f53e77a0e2f6c2cd7281ed',1,'dev::decryptSymNoAuth(SecureFixedHash&lt; 16 &gt; const &amp;_k, h128 const &amp;_iv, bytesConstRef _cipher)']]],
  ['defaultconstructcopyablemovable_4478',['DefaultConstructCopyableMovable',['../structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a31db61ce7d91cf934eb10dc70b0d4672',1,'taraxa::util::DefaultConstructCopyableMovable::DefaultConstructCopyableMovable(DefaultConstructCopyableMovable &amp;&amp;) noexcept(noexcept(T()))'],['../structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a3dc6fb187801345b1d2991e0dd48865a',1,'taraxa::util::DefaultConstructCopyableMovable::DefaultConstructCopyableMovable(const DefaultConstructCopyableMovable &amp;) noexcept(noexcept(T()))'],['../structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a855a78293e92333fe5609f47714a79b5',1,'taraxa::util::DefaultConstructCopyableMovable::DefaultConstructCopyableMovable(T val)'],['../structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a11e4826e5170008b2efe4c5ac309c4c1',1,'taraxa::util::DefaultConstructCopyableMovable::DefaultConstructCopyableMovable()=default']]],
  ['deinitlogging_4479',['DeinitLogging',['../classtaraxa_1_1logger_1_1_config.html#a652c68185a2b83da8df8fb2aff877050',1,'taraxa::logger::Config']]],
  ['delayedpbftsync_4480',['delayedPbftSync',['../classtaraxa_1_1network_1_1tarcap_1_1_pbft_sync_packet_handler.html#aa9247f9272968cb79832ac4e5e2cf533',1,'taraxa::network::tarcap::PbftSyncPacketHandler']]],
  ['delegation_5fdelay_4481',['delegation_delay',['../group___final_chain.html#aac2506da98467db60ef3751c2eab1391',1,'taraxa::final_chain::FinalChain::delegation_delay()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#a4b8f0831630b3578c70f4e67a4f97ef6',1,'taraxa::final_chain::FinalChainImpl::delegation_delay()']]],
  ['deletesnapshot_4482',['deleteSnapshot',['../classtaraxa_1_1_db_storage.html#a77cc4b74d4441edac750b95e824ae791',1,'taraxa::DbStorage']]],
  ['determinenewround_4483',['determineNewRound',['../group___vote.html#a0040cd4fb2c849752421dffcc062d430',1,'taraxa::VoteManager']]],
  ['determinepublic_4484',['determinePublic',['../structdev_1_1p2p_1_1_host.html#a24453cf74468379aef451da129e60c5f',1,'dev::p2p::Host']]],
  ['dev_5fsimple_5fexception_4485',['DEV_SIMPLE_EXCEPTION',['../classtaraxa_1_1net_1_1rpc_1_1eth_1_1_eth_impl.html#aa1dfa4c575f45bec4fa4df0aa9bbf966',1,'taraxa::net::rpc::eth::EthImpl::DEV_SIMPLE_EXCEPTION()'],['../namespacedev_1_1p2p.html#a85786a26bcad5c09f3a9b50a5c49010e',1,'dev::p2p::DEV_SIMPLE_EXCEPTION()'],['../namespacedev.html#a28d0d371bcd53a79ec6e025388dfaf26',1,'dev::DEV_SIMPLE_EXCEPTION()'],['../namespacetaraxa_1_1net_1_1rpc_1_1eth.html#aabff23a169ec37c540ea054fab5a53c5',1,'taraxa::net::rpc::eth::DEV_SIMPLE_EXCEPTION()'],['../namespacedev_1_1p2p.html#aec40319de46b05b7b29b2a9478b6d460',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRSecp256k1NotFound)'],['../namespacedev_1_1p2p.html#ac743c370e38c9d1acf4615f4ad712952',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRUnknownIdentityScheme)'],['../namespacedev_1_1p2p.html#a519e1383f50cb65ee37e9ff6a41fcfd7',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRKeysAreNotUniqueSorted)'],['../namespacedev_1_1p2p.html#a8d50f64ef8389816348aedd9ad2eb893',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRSignatureIsInvalid)'],['../namespacedev_1_1p2p.html#a360273a2e9c9c6aaceb502cbaf914abd',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRIsTooBig)'],['../namespacedev_1_1crypto.html#af2a12478709295407ccc4d93a073bdfc',1,'dev::crypto::DEV_SIMPLE_EXCEPTION(CryptoException)'],['../namespacedev_1_1crypto.html#a15bc8df921a5e850704474ba2b3a5e59',1,'dev::crypto::DEV_SIMPLE_EXCEPTION(InvalidState)'],['../namespacedev.html#a0b8085cfa4ce4c6d2dbe50a8d2bd458a',1,'dev::DEV_SIMPLE_EXCEPTION(WaitTimeout)'],['../namespacedev.html#a44ea2523d657094dd61589093b5a04bb',1,'dev::DEV_SIMPLE_EXCEPTION(ExternalFunctionFailure)'],['../namespacedev.html#a0abd1e289895c041811e1476d306ece1',1,'dev::DEV_SIMPLE_EXCEPTION(InterfaceNotSupported)'],['../namespacedev.html#a284d5976ac6f74a28637a11ff0471120',1,'dev::DEV_SIMPLE_EXCEPTION(WrongFieldType)'],['../namespacedev.html#ab900e3615719a0cf136272805113dd30',1,'dev::DEV_SIMPLE_EXCEPTION(MissingField)'],['../namespacedev.html#a502ea1315515149073c33d6ade682886',1,'dev::DEV_SIMPLE_EXCEPTION(UnknownField)'],['../namespacedev.html#ab9d75b42d78d7bba74f2684e4cd6cf96',1,'dev::DEV_SIMPLE_EXCEPTION(ValueTooLarge)'],['../namespacedev.html#a9c9aef151a4dabe39f87ac0365b8ec64',1,'dev::DEV_SIMPLE_EXCEPTION(FailedInvariant)'],['../namespacedev.html#ac0ed772efe313ef5158987349dbd678f',1,'dev::DEV_SIMPLE_EXCEPTION(Overflow)'],['../namespacedev.html#a327de80dab18a48aa368ab844bc395b5',1,'dev::DEV_SIMPLE_EXCEPTION(FileError)'],['../namespacedev.html#a8bd5764483662329f2f83dccc90bb13a',1,'dev::DEV_SIMPLE_EXCEPTION(BadRoot)'],['../namespacedev.html#a1eec65a70ee38e4e2715844253809228',1,'dev::DEV_SIMPLE_EXCEPTION(RootNotFound)'],['../namespacedev.html#a83f076111496c324f745edb92ca32773',1,'dev::DEV_SIMPLE_EXCEPTION(NoUPnPDevice)'],['../namespacedev.html#a3bc6cd2e673d17317a6f9ad877f33f23',1,'dev::DEV_SIMPLE_EXCEPTION(BadHexCharacter)']]],
  ['dev_5fsimple_5fexception_5frlp_4486',['DEV_SIMPLE_EXCEPTION_RLP',['../namespacedev.html#ad500bc05735dd2f89883fd0657f1897c',1,'dev::DEV_SIMPLE_EXCEPTION_RLP(UndersizeRLP)'],['../namespacedev.html#a926847180de7bc693356b6c9eea6fff0',1,'dev::DEV_SIMPLE_EXCEPTION_RLP(OversizeRLP)'],['../namespacedev.html#a0ec8a6a96f1b00a388296a2b515118c4',1,'dev::DEV_SIMPLE_EXCEPTION_RLP(BadRLP)'],['../namespacedev.html#a7142a9d865619e40e469bf11236a2813',1,'dev::DEV_SIMPLE_EXCEPTION_RLP(BadCast)']]],
  ['diff_4487',['diff',['../namespacedev.html#ae706766323baacdbdbe9ddb221e0cd92',1,'dev']]],
  ['difficulty_4488',['difficulty',['../group___final_chain.html#a992dc52632834c876af8ba07aa474028',1,'taraxa::final_chain::BlockHeader']]],
  ['dirnamefromfile_4489',['dirNameFromFile',['../classtaraxa_1_1cli_1_1_config.html#a34933d60f04538b88a80bccff8f95bff',1,'taraxa::cli::Config']]],
  ['disablesnapshots_4490',['disableSnapshots',['../classtaraxa_1_1_db_storage.html#a1389c4d576ca86153be5668efdff49de',1,'taraxa::DbStorage']]],
  ['disconnect_4491',['disconnect',['../structdev_1_1p2p_1_1_host.html#a2a5364b0c6d1c7fd2793f15774e084a4',1,'dev::p2p::Host::disconnect()'],['../structdev_1_1p2p_1_1_session.html#afc4ab254ddf7e74c2b814a1697ebcce2',1,'dev::p2p::Session::disconnect()'],['../structdev_1_1p2p_1_1_u_d_p_socket_face.html#ac35c32537a1f553fae7690aec96df087',1,'dev::p2p::UDPSocketFace::disconnect()'],['../classdev_1_1p2p_1_1_u_d_p_socket.html#af8ee5b2d1ec1ccb9c6e51a057b103f5b',1,'dev::p2p::UDPSocket::disconnect()'],['../classtaraxa_1_1network_1_1tarcap_1_1_packet_handler.html#a60a25f09bb4858e081bdfcde7a1361a0',1,'taraxa::network::tarcap::PacketHandler::disconnect()']]],
  ['disconnect_5f_4492',['disconnect_',['../structdev_1_1p2p_1_1_session.html#ad7df2f6c683c28fcdee77b9d695cecce',1,'dev::p2p::Session']]],
  ['disconnectwitherror_4493',['disconnectWithError',['../classdev_1_1p2p_1_1_u_d_p_socket.html#a217844e4646e15f30c5b1fe7ad8a372c',1,'dev::p2p::UDPSocket']]],
  ['discoverydatagram_4494',['DiscoveryDatagram',['../structdev_1_1p2p_1_1_discovery_datagram.html#aaf5f09106022dafba70fb4226881bf9c',1,'dev::p2p::DiscoveryDatagram::DiscoveryDatagram()'],['../structdev_1_1p2p_1_1_ping_node.html#a896187cbb24b61ba1d23ef98da134a88',1,'dev::p2p::PingNode::DiscoveryDatagram(bi::udp::endpoint const &amp;_from, NodeID const &amp;_fromid, h256 const &amp;_echo)'],['../structdev_1_1p2p_1_1_ping_node.html#aaf5f09106022dafba70fb4226881bf9c',1,'dev::p2p::PingNode::DiscoveryDatagram(bi::udp::endpoint const &amp;_to)'],['../structdev_1_1p2p_1_1_discovery_datagram.html#a896187cbb24b61ba1d23ef98da134a88',1,'dev::p2p::DiscoveryDatagram::DiscoveryDatagram()']]],
  ['distance_4495',['distance',['../classdev_1_1p2p_1_1_node_table.html#a2aa17544301359305d2c4fa87f7ae810',1,'dev::p2p::NodeTable']]],
  ['do_5faccept_4496',['do_accept',['../classtaraxa_1_1net_1_1_ws_server.html#aad26dbf80dab4ef48a5fd9321b168dc9',1,'taraxa::net::WsServer']]],
  ['do_5fdiscov_4497',['do_discov',['../structdev_1_1p2p_1_1_host.html#aedc2485cf79df61d01fa34af8a0c03b6',1,'dev::p2p::Host']]],
  ['do_5fread_4498',['do_read',['../classtaraxa_1_1net_1_1_ws_session.html#a72434d357dad6f1b9446f46110afe6bf',1,'taraxa::net::WsSession']]],
  ['do_5fwork_4499',['do_work',['../structdev_1_1p2p_1_1_host.html#a4d01c5e35e506c1ab3a51e8fc9c58d9f',1,'dev::p2p::Host']]],
  ['dodiscovery_4500',['doDiscovery',['../classdev_1_1p2p_1_1_node_table.html#a9e312771d262e0b4e8a02a4a45fcfeb6',1,'dev::p2p::NodeTable']]],
  ['dodiscoveryround_4501',['doDiscoveryRound',['../classdev_1_1p2p_1_1_node_table.html#a16ffc37a4d57247b6a3df373722dfc08',1,'dev::p2p::NodeTable']]],
  ['doendpointtracking_4502',['doEndpointTracking',['../classdev_1_1p2p_1_1_node_table.html#a008f3077a46a7dbc8cd14cc00f76aa49',1,'dev::p2p::NodeTable']]],
  ['dohandletimeouts_4503',['doHandleTimeouts',['../classdev_1_1p2p_1_1_node_table.html#a6dea38ad360fe5a2eafbd822a0774ed0',1,'dev::p2p::NodeTable']]],
  ['doread_4504',['doRead',['../structdev_1_1p2p_1_1_session.html#ae5897e387a91a07fb6fb184dd454c391',1,'dev::p2p::Session::doRead()'],['../classdev_1_1p2p_1_1_u_d_p_socket.html#a817936d11790fed60b5ecc3b18bad60a',1,'dev::p2p::UDPSocket::doRead()']]],
  ['dowrite_4505',['doWrite',['../classdev_1_1p2p_1_1_u_d_p_socket.html#aca5a57f30d9094b0f2a3cd96ed2a48d2',1,'dev::p2p::UDPSocket']]],
  ['dpos_5feligible_5ftotal_5fvote_5fcount_4506',['dpos_eligible_total_vote_count',['../group___final_chain.html#afa0df0c96982e3fb920fae205e3ef0d9',1,'taraxa::final_chain::FinalChain::dpos_eligible_total_vote_count()'],['../group___final_chain.html#a4f67cd91d38cc3f873506b94d66ee916',1,'taraxa::state_api::StateAPI::dpos_eligible_total_vote_count()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#afd9369027b9b22c13e82d22ac9f07686',1,'taraxa::final_chain::FinalChainImpl::dpos_eligible_total_vote_count()']]],
  ['dpos_5feligible_5fvote_5fcount_4507',['dpos_eligible_vote_count',['../group___final_chain.html#a92cea442deb2f451264773571de49e06',1,'taraxa::final_chain::FinalChain::dpos_eligible_vote_count()'],['../group___final_chain.html#a954304ae0af99b51d175d9ba4a09a976',1,'taraxa::state_api::StateAPI::dpos_eligible_vote_count()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#a7935a9e52c1d4ae3ae791c1be0a2e936',1,'taraxa::final_chain::FinalChainImpl::dpos_eligible_vote_count()']]],
  ['dpos_5fget_5fvrf_5fkey_4508',['dpos_get_vrf_key',['../group___final_chain.html#a36c579a28c11da0a88eabf8cbf601770',1,'taraxa::state_api::StateAPI::dpos_get_vrf_key()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#a9ff1eeb365ce30d17f259213df2e72d4',1,'taraxa::final_chain::FinalChainImpl::dpos_get_vrf_key()'],['../group___final_chain.html#ab193319e8081030abcfa62a172ab6cfd',1,'taraxa::final_chain::FinalChain::dpos_get_vrf_key(EthBlockNumber blk_n, const addr_t &amp;addr) const =0']]],
  ['dpos_5fis_5feligible_4509',['dpos_is_eligible',['../group___final_chain.html#a218a8d86259ff424fdbda18138fb0bad',1,'taraxa::final_chain::FinalChain::dpos_is_eligible()'],['../group___final_chain.html#a87694ae315e7ad895031180ea979bd76',1,'taraxa::state_api::StateAPI::dpos_is_eligible()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#ab3d690c5e49bc20d724a4f041aa9e6f9',1,'taraxa::final_chain::FinalChainImpl::dpos_is_eligible()']]],
  ['drawgraph_4510',['drawGraph',['../group___d_a_g.html#aa790302c9ac6ce992850dd09facdfc83',1,'taraxa::Dag::drawGraph()'],['../group___d_a_g.html#a4338078fd5288067701b3f276076c01a',1,'taraxa::DagManager::drawGraph(std::string const &amp;dotfile) const']]],
  ['drawpivotgraph_4511',['drawPivotGraph',['../group___d_a_g.html#a6358aeccb8b0ed554c321dda03a02768',1,'taraxa::DagManager']]],
  ['drawtotalgraph_4512',['drawTotalGraph',['../group___d_a_g.html#a4ab4fb411fe6447b8a556a53c1ea700c',1,'taraxa::DagManager']]],
  ['drop_4513',['drop',['../structdev_1_1p2p_1_1_session.html#a7d6f8bf80f525a3a26da89f6bd892b81',1,'dev::p2p::Session']]],
  ['dropnode_4514',['dropNode',['../classdev_1_1p2p_1_1_node_table.html#a7ab7bf299174016952f720ed9d682013',1,'dev::p2p::NodeTable']]],
  ['dry_5frun_5ftransaction_4515',['dry_run_transaction',['../group___final_chain.html#ae8310bd008463bad92c1fa31849e8d2a',1,'taraxa::state_api::StateAPI']]],
  ['duration_4516',['duration',['../classdev_1_1_timer.html#a7094444ed85065f0e8db6226f11b108a',1,'dev::Timer']]]
];
