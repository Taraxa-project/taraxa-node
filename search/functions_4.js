var searchData=
[
  ['dag_4814',['Dag',['../group___d_a_g.html#a6266d656a7eba53bac6fa8dc7e7eb5da',1,'taraxa::Dag::Dag(const Dag &amp;)=default'],['../group___d_a_g.html#a91d4f1eb81776e18f3908374d0eef614',1,'taraxa::Dag::Dag(Dag &amp;&amp;)=default'],['../group___d_a_g.html#a7def37042439fa3c713ea8861d40b199',1,'taraxa::Dag::Dag(blk_hash_t const &amp;dag_genesis_block_hash, addr_t node_addr)']]],
  ['dagblock_4815',['DagBlock',['../classgraphql_1_1taraxa_1_1_dag_block.html#aec225d9ef3cc0e84f0f87242f20f6e3f',1,'graphql::taraxa::DagBlock::DagBlock()'],['../classtaraxa_1_1_dag_block.html#a14040cdc9db1c166b9c0d021ce9bf15d',1,'taraxa::DagBlock::DagBlock()=default'],['../classtaraxa_1_1_dag_block.html#ad178f6fc15e23d4486dbfcaa5dc12136',1,'taraxa::DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, uint64_t est, sig_t signature, blk_hash_t hash, addr_t sender)'],['../classtaraxa_1_1_dag_block.html#acfc470101e8d0dcdc4e1ed2f7e3f755e',1,'taraxa::DagBlock::DagBlock(blk_hash_t pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, sig_t signature, blk_hash_t hash, addr_t sender)'],['../classtaraxa_1_1_dag_block.html#a00e51bfd48cfa3dab7357938cb4e65b0',1,'taraxa::DagBlock::DagBlock(blk_hash_t const &amp;pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, secret_t const &amp;sk)'],['../classtaraxa_1_1_dag_block.html#a5918ea1955486417267213c3e9a12a74',1,'taraxa::DagBlock::DagBlock(blk_hash_t const &amp;pivot, level_t level, vec_blk_t tips, vec_trx_t trxs, uint64_t est, VdfSortition vdf, secret_t const &amp;sk)'],['../classtaraxa_1_1_dag_block.html#ad780ba7392776f8e2c1c8b80c414cc82',1,'taraxa::DagBlock::DagBlock(Json::Value const &amp;doc)'],['../classtaraxa_1_1_dag_block.html#a1f73650dd7f598ac35927a145f8a81a3',1,'taraxa::DagBlock::DagBlock(string const &amp;json)'],['../classtaraxa_1_1_dag_block.html#afb5a52db03e70fafdb1d35d56e972806',1,'taraxa::DagBlock::DagBlock(dev::RLP const &amp;_rlp)'],['../classtaraxa_1_1_dag_block.html#a92d0d1d65b53c557b39f06aaae6f9b47',1,'taraxa::DagBlock::DagBlock(dev::bytes const &amp;_rlp)']]],
  ['dagblockfromdagpacket_4816',['dagBlockFromDagPacket',['../classtaraxa_1_1network_1_1threadpool_1_1_packets_blocking_mask.html#a0c094657b6f79256d9fd229fc9e5939d',1,'taraxa::network::threadpool::PacketsBlockingMask']]],
  ['dagblockindb_4817',['dagBlockInDb',['../classtaraxa_1_1_db_storage.html#a0e7339685ce0f30cb9186ccec58709fa',1,'taraxa::DbStorage']]],
  ['dagblockpackethandler_4818',['DagBlockPacketHandler',['../classtaraxa_1_1network_1_1tarcap_1_1_dag_block_packet_handler.html#ab78ee7b487271b0b2591929cf089fef0',1,'taraxa::network::tarcap::DagBlockPacketHandler']]],
  ['dagblockproposer_4819',['DagBlockProposer',['../group___d_a_g.html#a32b558c1ef9413eca9b144811b1538b7',1,'taraxa::DagBlockProposer::DagBlockProposer(const DagBlockProposerConfig &amp;bp_config, std::shared_ptr&lt; DagManager &gt; dag_mgr, std::shared_ptr&lt; TransactionManager &gt; trx_mgr, std::shared_ptr&lt; final_chain::FinalChain &gt; final_chain, std::shared_ptr&lt; DbStorage &gt; db, std::shared_ptr&lt; KeyManager &gt; key_manager, addr_t node_addr, secret_t node_sk, vrf_wrapper::vrf_sk_t vrf_sk, uint64_t pbft_gas_limit, uint64_t dag_gas_limit, const state_api::Config &amp;state_config)'],['../group___d_a_g.html#aace097ada959a377e46c6a6e57c43c21',1,'taraxa::DagBlockProposer::DagBlockProposer(const DagBlockProposer &amp;)=delete'],['../group___d_a_g.html#ab6d80eccedea603e03d4a194c5df0327',1,'taraxa::DagBlockProposer::DagBlockProposer(DagBlockProposer &amp;&amp;)=delete']]],
  ['dagfrontier_4820',['DagFrontier',['../structtaraxa_1_1_dag_frontier.html#ab0af91682b5f811569387e80042179d6',1,'taraxa::DagFrontier::DagFrontier()=default'],['../structtaraxa_1_1_dag_frontier.html#a5956303317c4cd51a01c79919a38b20c',1,'taraxa::DagFrontier::DagFrontier(blk_hash_t const &amp;pivot, vec_blk_t const &amp;tips)']]],
  ['dagmanager_4821',['DagManager',['../group___d_a_g.html#a235e8769bbfc5b0a0cc5f4a3df9087d9',1,'taraxa::DagManager::DagManager(const DagBlock &amp;dag_genesis_block, addr_t node_addr, const SortitionConfig &amp;sortition_config, const DagConfig &amp;dag_config, std::shared_ptr&lt; TransactionManager &gt; trx_mgr, std::shared_ptr&lt; PbftChain &gt; pbft_chain, std::shared_ptr&lt; FinalChain &gt; final_chain, std::shared_ptr&lt; DbStorage &gt; db, std::shared_ptr&lt; KeyManager &gt; key_manager, uint64_t pbft_gas_limit, const state_api::Config &amp;state_config, bool is_light_node=false, uint64_t light_node_history=0, uint32_t max_levels_per_period=kMaxLevelsPerPeriod, uint32_t dag_expiry_limit=kDagExpiryLevelLimit)'],['../group___d_a_g.html#aa0fe82cc22a187cab97099ef465bed02',1,'taraxa::DagManager::DagManager(const DagManager &amp;)=delete'],['../group___d_a_g.html#a1a957529304d407e997ca611b4530ab7',1,'taraxa::DagManager::DagManager(DagManager &amp;&amp;)=delete']]],
  ['dagsyncingallowed_4822',['dagSyncingAllowed',['../classtaraxa_1_1network_1_1tarcap_1_1_taraxa_peer.html#a2ca9c17057d092453dd4e0337a27bafe',1,'taraxa::network::tarcap::TaraxaPeer']]],
  ['dagsyncpackethandler_4823',['DagSyncPacketHandler',['../classtaraxa_1_1network_1_1tarcap_1_1_dag_sync_packet_handler.html#aea6e16e423cfc67def901372855f1254',1,'taraxa::network::tarcap::DagSyncPacketHandler']]],
  ['data_4824',['data',['../classdev_1_1_fixed_hash.html#a49662c17a82b8cdeb1a81d2244c61e13',1,'dev::FixedHash::data()'],['../classdev_1_1p2p_1_1_r_l_p_x_packet.html#a42dfeb2ea6d5911070d732806697f9a4',1,'dev::p2p::RLPXPacket::data()'],['../classdev_1_1vector__ref.html#adadc63c541b917aeb70d3385fad42ec2',1,'dev::vector_ref::data()'],['../classdev_1_1_secure_fixed_hash.html#abc47be5c36cc816ed6b0d6517fe97c3c',1,'dev::SecureFixedHash::data()'],['../classdev_1_1_fixed_hash.html#a7aa90cb5b66e28cf93b841af6da80c6a',1,'dev::FixedHash::data()'],['../classdev_1_1_r_l_p.html#ae1f6d3577e4d896ba845933fa71a4b35',1,'dev::RLP::data()']]],
  ['dbexception_4825',['DbException',['../classtaraxa_1_1_db_exception.html#a7686db339de4857e8acf8a7fa8994751',1,'taraxa::DbException::DbException(const std::string &amp;desc)'],['../classtaraxa_1_1_db_exception.html#a855ce1390542835d262d3a67a1bea980',1,'taraxa::DbException::DbException(const DbException &amp;)=default'],['../classtaraxa_1_1_db_exception.html#a9dc5cb437b424f6d423d47d173d8165a',1,'taraxa::DbException::DbException(DbException &amp;&amp;)=default']]],
  ['dbstorage_4826',['DbStorage',['../classtaraxa_1_1_db_storage.html#aa41cccbaab43c81d7297013102e8f83c',1,'taraxa::DbStorage::DbStorage(fs::path const &amp;base_path, uint32_t db_snapshot_each_n_pbft_block=0, uint32_t max_open_files=0, uint32_t db_max_snapshots=0, PbftPeriod db_revert_to_period=0, addr_t node_addr=addr_t(), bool rebuild=false)'],['../classtaraxa_1_1_db_storage.html#af1708845f17561a106abe94391a964f7',1,'taraxa::DbStorage::DbStorage(const DbStorage &amp;)=delete'],['../classtaraxa_1_1_db_storage.html#a79a9960113464b742cc432cd57415486',1,'taraxa::DbStorage::DbStorage(DbStorage &amp;&amp;)=delete']]],
  ['dbstoragepath_4827',['dbStoragePath',['../classtaraxa_1_1_db_storage.html#a9b3cbfc1c55197bafc743ffc9f886ab5',1,'taraxa::DbStorage']]],
  ['dbversion_4828',['dbVersion',['../classtaraxa_1_1storage_1_1migration_1_1_transaction_period.html#af33f346cbef1960ec4ed8b672f566bbb',1,'taraxa::storage::migration::TransactionPeriod::dbVersion()'],['../classtaraxa_1_1storage_1_1migration_1_1_base.html#a0d297d52410384d72e8d7283abbdaf5e',1,'taraxa::storage::migration::Base::dbVersion()']]],
  ['debug_4829',['Debug',['../classtaraxa_1_1net_1_1_debug.html#ae52c9ee7c3fc2c69af9bf6dd293629a8',1,'taraxa::net::Debug']]],
  ['debug_5fdpostotalamountdelegated_4830',['debug_dposTotalAmountDelegated',['../classtaraxa_1_1net_1_1_debug.html#a193c8854215b16f01b198b9159293ae0',1,'taraxa::net::Debug::debug_dposTotalAmountDelegated()'],['../classtaraxa_1_1net_1_1_debug_client.html#ade2ccff99ac7d649dfffb560f41eecf7',1,'taraxa::net::DebugClient::debug_dposTotalAmountDelegated()'],['../classtaraxa_1_1net_1_1_debug_face.html#a86b4b016515907bd468cfd0a712ad074',1,'taraxa::net::DebugFace::debug_dposTotalAmountDelegated(const std::string &amp;param1)=0']]],
  ['debug_5fdpostotalamountdelegatedi_4831',['debug_dposTotalAmountDelegatedI',['../classtaraxa_1_1net_1_1_debug_face.html#a2d3a4ce41504d535df357dee02fedbab',1,'taraxa::net::DebugFace']]],
  ['debug_5fdposvalidatortotalstakes_4832',['debug_dposValidatorTotalStakes',['../classtaraxa_1_1net_1_1_debug.html#a596ec3408460def2bb43b4da9e06df18',1,'taraxa::net::Debug::debug_dposValidatorTotalStakes()'],['../classtaraxa_1_1net_1_1_debug_client.html#a01589f9ca5706bb9779ac16ab06189bd',1,'taraxa::net::DebugClient::debug_dposValidatorTotalStakes()'],['../classtaraxa_1_1net_1_1_debug_face.html#a924ebd853c809107e3ef422894b3b3fb',1,'taraxa::net::DebugFace::debug_dposValidatorTotalStakes(const std::string &amp;param1)=0']]],
  ['debug_5fdposvalidatortotalstakesi_4833',['debug_dposValidatorTotalStakesI',['../classtaraxa_1_1net_1_1_debug_face.html#a167732e41991961e491ed3f63aa6ced9',1,'taraxa::net::DebugFace']]],
  ['debug_5fgetperioddagblocks_4834',['debug_getPeriodDagBlocks',['../classtaraxa_1_1net_1_1_debug_client.html#a8a49ef866539508c238271fab43870ae',1,'taraxa::net::DebugClient::debug_getPeriodDagBlocks()'],['../classtaraxa_1_1net_1_1_debug_face.html#ae44417f9ba494eca1e8ead23edb65698',1,'taraxa::net::DebugFace::debug_getPeriodDagBlocks()'],['../classtaraxa_1_1net_1_1_debug.html#ab9ea195630ec06900ca67adf1a320516',1,'taraxa::net::Debug::debug_getPeriodDagBlocks()']]],
  ['debug_5fgetperioddagblocksi_4835',['debug_getPeriodDagBlocksI',['../classtaraxa_1_1net_1_1_debug_face.html#a018252e6c233d044ee2dc3216fa805a9',1,'taraxa::net::DebugFace']]],
  ['debug_5fgetperiodtransactionswithreceipts_4836',['debug_getPeriodTransactionsWithReceipts',['../classtaraxa_1_1net_1_1_debug.html#ab5da92d536ed095b93d72b8441a8f42d',1,'taraxa::net::Debug::debug_getPeriodTransactionsWithReceipts()'],['../classtaraxa_1_1net_1_1_debug_client.html#a7220c05786d04075274a94ba00c2c863',1,'taraxa::net::DebugClient::debug_getPeriodTransactionsWithReceipts()'],['../classtaraxa_1_1net_1_1_debug_face.html#aeee7ba1e8e9a82aec57332efbfa017df',1,'taraxa::net::DebugFace::debug_getPeriodTransactionsWithReceipts(const std::string &amp;param1)=0']]],
  ['debug_5fgetperiodtransactionswithreceiptsi_4837',['debug_getPeriodTransactionsWithReceiptsI',['../classtaraxa_1_1net_1_1_debug_face.html#ad3483fb64ec5d37cb51781e6b12820ac',1,'taraxa::net::DebugFace']]],
  ['debug_5fgetpreviousblockcertvotes_4838',['debug_getPreviousBlockCertVotes',['../classtaraxa_1_1net_1_1_debug.html#a2d8e5c4e50fe689e781591b4ba95e296',1,'taraxa::net::Debug::debug_getPreviousBlockCertVotes()'],['../classtaraxa_1_1net_1_1_debug_client.html#ae77e9b3c5b344ad28925de443f3625be',1,'taraxa::net::DebugClient::debug_getPreviousBlockCertVotes()'],['../classtaraxa_1_1net_1_1_debug_face.html#a70832367398834783aa9721022f3016d',1,'taraxa::net::DebugFace::debug_getPreviousBlockCertVotes(const std::string &amp;param1)=0']]],
  ['debug_5fgetpreviousblockcertvotesi_4839',['debug_getPreviousBlockCertVotesI',['../classtaraxa_1_1net_1_1_debug_face.html#ab3fabc7f4cb811646751de0eeb3b5a1d',1,'taraxa::net::DebugFace']]],
  ['debug_5ftracecall_4840',['debug_traceCall',['../classtaraxa_1_1net_1_1_debug.html#a85f1d4335d4676a3735967ad0d2cfbea',1,'taraxa::net::Debug::debug_traceCall()'],['../classtaraxa_1_1net_1_1_debug_client.html#aaea8a1290c23a206d32a975a12d92bc5',1,'taraxa::net::DebugClient::debug_traceCall()'],['../classtaraxa_1_1net_1_1_debug_face.html#a7ddab247275cc6a4272b888989bcb8ab',1,'taraxa::net::DebugFace::debug_traceCall(const Json::Value &amp;param1, const std::string &amp;param2)=0']]],
  ['debug_5ftracecalli_4841',['debug_traceCallI',['../classtaraxa_1_1net_1_1_debug_face.html#a5cf8ba06fbaf6bba56c7df8778546db0',1,'taraxa::net::DebugFace']]],
  ['debug_5ftracetransaction_4842',['debug_traceTransaction',['../classtaraxa_1_1net_1_1_debug.html#a3daf730fcb91d8266c2eb4f327741289',1,'taraxa::net::Debug::debug_traceTransaction()'],['../classtaraxa_1_1net_1_1_debug_client.html#aa50bd584eff17885f5b20ac3534dcf15',1,'taraxa::net::DebugClient::debug_traceTransaction()'],['../classtaraxa_1_1net_1_1_debug_face.html#a1546ee4d5b1a0dd831696902b4a0ba95',1,'taraxa::net::DebugFace::debug_traceTransaction(const std::string &amp;param1)=0']]],
  ['debug_5ftracetransactioni_4843',['debug_traceTransactionI',['../classtaraxa_1_1net_1_1_debug_face.html#ad2128a5ef7de3b36ed5e34031b6b965c',1,'taraxa::net::DebugFace']]],
  ['debugclient_4844',['DebugClient',['../classtaraxa_1_1net_1_1_debug_client.html#a0176a459e0cda8993a9150875c416174',1,'taraxa::net::DebugClient']]],
  ['debugface_4845',['DebugFace',['../classtaraxa_1_1net_1_1_debug_face.html#ac7c7de8a883f6443df79aa9d5b38506b',1,'taraxa::net::DebugFace']]],
  ['dec_5fddos_5fprotection_5fconfig_5fjson_4846',['dec_ddos_protection_config_json',['../namespacetaraxa.html#aa8f416533f421d87c50990a6aa205f6a',1,'taraxa']]],
  ['dec_5fjson_4847',['dec_json',['../namespacetaraxa_1_1state__api.html#a8430d4247900167bc1ac842c70484cc3',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, DPOSConfig &amp;obj)'],['../namespacetaraxa_1_1state__api.html#a71678cb3f5feb67a738df3d7d2f45270',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, Config &amp;obj)'],['../hardfork_8cpp.html#a3d203167ce341d6ad0ebba86baf013fc',1,'dec_json(const Json::Value &amp;json, Redelegation &amp;obj):&#160;hardfork.cpp'],['../hardfork_8cpp.html#ae6551b937467c5972009ffa1ac192aec',1,'dec_json(const Json::Value &amp;json, MagnoliaHardfork &amp;obj):&#160;hardfork.cpp'],['../hardfork_8cpp.html#a7f5bc0b7b5eec75c54920eefa9816f71',1,'dec_json(const Json::Value &amp;json, AspenHardfork &amp;obj):&#160;hardfork.cpp'],['../hardfork_8cpp.html#aa0a41c5ffaae749875f71651deb8f088',1,'dec_json(const Json::Value &amp;json, FicusHardforkConfig &amp;obj):&#160;hardfork.cpp'],['../namespacetaraxa.html#ad2584bf1da77275f513d1378d392fd99',1,'taraxa::dec_json()'],['../hardfork_8cpp.html#aee3750fcf3856743f5e71f64f88ad959',1,'dec_json():&#160;hardfork.cpp'],['../namespacetaraxa.html#aad70d4845a45158f0122129ce50737b7',1,'taraxa::dec_json(const Json::Value &amp;json, PrometheusConfig &amp;config)'],['../namespacetaraxa.html#aab796ac6602a0ecde1f58bc181127628',1,'taraxa::dec_json(const Json::Value &amp;json)'],['../namespacetaraxa.html#a6ea909654b6cd15b58987ec4d1a9d0ab',1,'taraxa::dec_json(const Json::Value &amp;json, VrfParams &amp;obj)'],['../namespacetaraxa.html#a2850298ee300bd104ca9db9352abf63f',1,'taraxa::dec_json(const Json::Value &amp;json, VdfParams &amp;obj)'],['../namespacetaraxa.html#a124545b9fa417446dd410b9c69701744',1,'taraxa::dec_json(const Json::Value &amp;json, SortitionParams &amp;obj)'],['../namespacetaraxa.html#a0d1168f714806db0733d6a1749ae6618',1,'taraxa::dec_json(Json::Value const &amp;json, GenesisConfig &amp;obj)'],['../namespacetaraxa_1_1state__api.html#a88679bd672ad12f8a2517728f1c1109f',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, ValidatorInfo &amp;obj)'],['../namespacetaraxa_1_1state__api.html#a0cdba49b2802bab1d0da7a671714e77b',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, BalanceMap &amp;obj)'],['../namespacetaraxa_1_1state__api.html#a142767c95d8323471aee83d86833ee8c',1,'taraxa::state_api::dec_json(const Json::Value &amp;json, uint64_t chain_id, EVMChainConfig &amp;obj)'],['../namespacetaraxa.html#a6d4575c65b00ce4d77c66f41277ce170',1,'taraxa::dec_json(Json::Value const &amp;json, PbftConfig &amp;obj)'],['../namespacetaraxa.html#aa5e15ca58e0ad240f75a8c70883a00e0',1,'taraxa::dec_json(const Json::Value &amp;json, NetworkConfig &amp;network)'],['../namespacetaraxa.html#a74ad03c0d7a830dd89d7f10d6f941077',1,'taraxa::dec_json(const Json::Value &amp;json, ConnectionConfig &amp;config)'],['../hardfork_8hpp.html#aee3750fcf3856743f5e71f64f88ad959',1,'dec_json(const Json::Value &amp;json, HardforksConfig &amp;obj):&#160;hardfork.cpp'],['../hardfork_8hpp.html#aa0a41c5ffaae749875f71651deb8f088',1,'dec_json(const Json::Value &amp;json, FicusHardforkConfig &amp;obj):&#160;hardfork.cpp'],['../hardfork_8hpp.html#a7f5bc0b7b5eec75c54920eefa9816f71',1,'dec_json(const Json::Value &amp;json, AspenHardfork &amp;obj):&#160;hardfork.cpp'],['../hardfork_8hpp.html#ae6551b937467c5972009ffa1ac192aec',1,'dec_json(const Json::Value &amp;json, MagnoliaHardfork &amp;obj):&#160;hardfork.cpp'],['../hardfork_8hpp.html#a3d203167ce341d6ad0ebba86baf013fc',1,'dec_json(const Json::Value &amp;json, Redelegation &amp;obj):&#160;hardfork.cpp'],['../namespacetaraxa.html#a4fe5a976d3d25a7348800b50b8d377b5',1,'taraxa::dec_json(Json::Value const &amp;json, DBConfig &amp;db_config)'],['../namespacetaraxa.html#a681e859a51cfd602ec476273ae67ce91',1,'taraxa::dec_json(const Json::Value &amp;json, DagBlockProposerConfig &amp;obj)'],['../namespacetaraxa.html#a9c00e55e957ffb65d8891c88785cf57e',1,'taraxa::dec_json(const Json::Value &amp;json, DagConfig &amp;obj)'],['../namespacetaraxa.html#a1a7add6cdc2ba2969325ea1ad4723859',1,'taraxa::dec_json(Json::Value const &amp;json, GasPriceConfig &amp;obj)']]],
  ['decodepbftvotesbundlerlp_4848',['decodePbftVotesBundleRlp',['../group___vote.html#ga65e4a1c06bac4c8d40d08e730c526716',1,'taraxa']]],
  ['decodeperioddata_4849',['decodePeriodData',['../classtaraxa_1_1network_1_1tarcap_1_1_pbft_sync_packet_handler.html#ad1ca071b6524c90757e1c70635a4e1a1',1,'taraxa::network::tarcap::PbftSyncPacketHandler']]],
  ['decodepillarvotesbundlerlp_4850',['decodePillarVotesBundleRlp',['../group___vote.html#ga00061f355abe5cfd1a385f637a663c32',1,'taraxa']]],
  ['decoder_5fcb_5fc_4851',['decoder_cb_c',['../namespacetaraxa_1_1state__api.html#a21f5167bc7fd439ed8b01d89c1bd814e',1,'taraxa::state_api']]],
  ['decodesolidity_4852',['decodeSolidity',['../group___p_i_l_l_a_r___c_h_a_i_n.html#a238fc28d306b6b02a95c1ae1d1d42634',1,'taraxa::pillar_chain::PillarBlock::decodeSolidity()'],['../group___vote.html#a5d13ca0085fc1eee75ab3ef55704f9ab',1,'taraxa::PillarVote::decodeSolidity()']]],
  ['decodevotesbundle_4853',['decodeVotesBundle',['../classtaraxa_1_1network_1_1tarcap_1_1_pbft_sync_packet_handler.html#a421a2d55bd12dae380cfef81dda8d444',1,'taraxa::network::tarcap::PbftSyncPacketHandler']]],
  ['decompressframe_4854',['decompressFrame',['../classdev_1_1p2p_1_1_r_l_p_x_frame_coder.html#a477edddcaebca0ee2d9a39f9ea6d6c15',1,'dev::p2p::RLPXFrameCoder']]],
  ['decrementactworkerscount_4855',['decrementActWorkersCount',['../classtaraxa_1_1network_1_1threadpool_1_1_packets_queue.html#a65fb66a959376e6ace47e40595a19417',1,'taraxa::network::threadpool::PacketsQueue']]],
  ['decrypt_4856',['decrypt',['../classdev_1_1crypto_1_1_secp256k1_p_p.html#ad3358d52e135b75e3e86110c68051c64',1,'dev::crypto::Secp256k1PP::decrypt()'],['../namespacedev.html#a91ee12ea84c5b9166e35e9af56010e8c',1,'dev::decrypt(Secret const &amp;_k, bytesConstRef _cipher, bytes &amp;o_plaintext)']]],
  ['decryptaes128ctr_4857',['decryptAES128CTR',['../namespacedev.html#a68f8f8b4cd62360ce45a1b6ef467ce59',1,'dev']]],
  ['decryptecies_4858',['decryptECIES',['../namespacedev.html#a46bb4787ff142b9cd040af5b15334013',1,'dev::decryptECIES(Secret const &amp;_k, bytesConstRef _cipher, bytes &amp;o_plaintext)'],['../namespacedev.html#ae1ddd7b7ee60c0bc8d6e047b4b84d1e4',1,'dev::decryptECIES(Secret const &amp;_k, bytesConstRef _sharedMacData, bytesConstRef _cipher, bytes &amp;o_plaintext)'],['../classdev_1_1crypto_1_1_secp256k1_p_p.html#ac7ff99c05c14afd9354f3c2f841b1865',1,'dev::crypto::Secp256k1PP::decryptECIES(Secret const &amp;_k, bytesConstRef _sharedMacData, bytes &amp;io_text)'],['../classdev_1_1crypto_1_1_secp256k1_p_p.html#a8e0bfb8aaa2e339e88a893809eae0a9c',1,'dev::crypto::Secp256k1PP::decryptECIES(Secret const &amp;_k, bytes &amp;io_text)']]],
  ['decryptsymnoauth_4859',['decryptSymNoAuth',['../namespacedev.html#a3bef0b6d00f9f9b2b3a558e51cb9d81d',1,'dev::decryptSymNoAuth(SecureFixedHash&lt; 32 &gt; const &amp;_k, h128 const &amp;_iv, bytesConstRef _cipher)'],['../namespacedev.html#a34883d4634f53e77a0e2f6c2cd7281ed',1,'dev::decryptSymNoAuth(SecureFixedHash&lt; 16 &gt; const &amp;_k, h128 const &amp;_iv, bytesConstRef _cipher)']]],
  ['defaultconstructcopyablemovable_4860',['DefaultConstructCopyableMovable',['../structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a31db61ce7d91cf934eb10dc70b0d4672',1,'taraxa::util::DefaultConstructCopyableMovable::DefaultConstructCopyableMovable(DefaultConstructCopyableMovable &amp;&amp;) noexcept(noexcept(T()))'],['../structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a3dc6fb187801345b1d2991e0dd48865a',1,'taraxa::util::DefaultConstructCopyableMovable::DefaultConstructCopyableMovable(const DefaultConstructCopyableMovable &amp;) noexcept(noexcept(T()))'],['../structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a855a78293e92333fe5609f47714a79b5',1,'taraxa::util::DefaultConstructCopyableMovable::DefaultConstructCopyableMovable(T val)'],['../structtaraxa_1_1util_1_1_default_construct_copyable_movable.html#a11e4826e5170008b2efe4c5ac309c4c1',1,'taraxa::util::DefaultConstructCopyableMovable::DefaultConstructCopyableMovable()=default']]],
  ['deinitlogging_4861',['DeinitLogging',['../classtaraxa_1_1logger_1_1_config.html#a652c68185a2b83da8df8fb2aff877050',1,'taraxa::logger::Config']]],
  ['delayedpbftsync_4862',['delayedPbftSync',['../classtaraxa_1_1network_1_1tarcap_1_1_pbft_sync_packet_handler.html#aa9247f9272968cb79832ac4e5e2cf533',1,'taraxa::network::tarcap::PbftSyncPacketHandler']]],
  ['delegation_5fdelay_4863',['delegation_delay',['../group___final_chain.html#aac2506da98467db60ef3751c2eab1391',1,'taraxa::final_chain::FinalChain::delegation_delay()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#a4b8f0831630b3578c70f4e67a4f97ef6',1,'taraxa::final_chain::FinalChainImpl::delegation_delay()']]],
  ['deletecolumndata_4864',['deleteColumnData',['../classtaraxa_1_1_db_storage.html#a58f8f08182c66b2865c959221d5795f3',1,'taraxa::DbStorage']]],
  ['deletesnapshot_4865',['deleteSnapshot',['../classtaraxa_1_1_db_storage.html#a77cc4b74d4441edac750b95e824ae791',1,'taraxa::DbStorage']]],
  ['determinenewround_4866',['determineNewRound',['../group___vote.html#a0040cd4fb2c849752421dffcc062d430',1,'taraxa::VoteManager']]],
  ['determinepublic_4867',['determinePublic',['../structdev_1_1p2p_1_1_host.html#a24453cf74468379aef451da129e60c5f',1,'dev::p2p::Host']]],
  ['dev_5fsimple_5fexception_4868',['DEV_SIMPLE_EXCEPTION',['../namespacedev_1_1p2p.html#a519e1383f50cb65ee37e9ff6a41fcfd7',1,'dev::p2p::DEV_SIMPLE_EXCEPTION()'],['../classtaraxa_1_1net_1_1rpc_1_1eth_1_1_eth_impl.html#aa1dfa4c575f45bec4fa4df0aa9bbf966',1,'taraxa::net::rpc::eth::EthImpl::DEV_SIMPLE_EXCEPTION()'],['../namespacedev.html#a327de80dab18a48aa368ab844bc395b5',1,'dev::DEV_SIMPLE_EXCEPTION()'],['../namespacedev_1_1crypto.html#af2a12478709295407ccc4d93a073bdfc',1,'dev::crypto::DEV_SIMPLE_EXCEPTION(CryptoException)'],['../namespacedev_1_1crypto.html#a15bc8df921a5e850704474ba2b3a5e59',1,'dev::crypto::DEV_SIMPLE_EXCEPTION(InvalidState)'],['../namespacedev.html#a0b8085cfa4ce4c6d2dbe50a8d2bd458a',1,'dev::DEV_SIMPLE_EXCEPTION(WaitTimeout)'],['../namespacedev.html#a44ea2523d657094dd61589093b5a04bb',1,'dev::DEV_SIMPLE_EXCEPTION(ExternalFunctionFailure)'],['../namespacedev.html#a0abd1e289895c041811e1476d306ece1',1,'dev::DEV_SIMPLE_EXCEPTION(InterfaceNotSupported)'],['../namespacedev.html#a284d5976ac6f74a28637a11ff0471120',1,'dev::DEV_SIMPLE_EXCEPTION(WrongFieldType)'],['../namespacedev.html#ab900e3615719a0cf136272805113dd30',1,'dev::DEV_SIMPLE_EXCEPTION(MissingField)'],['../namespacedev.html#a502ea1315515149073c33d6ade682886',1,'dev::DEV_SIMPLE_EXCEPTION(UnknownField)'],['../namespacedev.html#ab9d75b42d78d7bba74f2684e4cd6cf96',1,'dev::DEV_SIMPLE_EXCEPTION(ValueTooLarge)'],['../namespacedev.html#a9c9aef151a4dabe39f87ac0365b8ec64',1,'dev::DEV_SIMPLE_EXCEPTION(FailedInvariant)'],['../namespacedev.html#ac0ed772efe313ef5158987349dbd678f',1,'dev::DEV_SIMPLE_EXCEPTION(Overflow)'],['../namespacedev.html#a8bd5764483662329f2f83dccc90bb13a',1,'dev::DEV_SIMPLE_EXCEPTION(BadRoot)'],['../namespacedev.html#a1eec65a70ee38e4e2715844253809228',1,'dev::DEV_SIMPLE_EXCEPTION(RootNotFound)'],['../namespacedev.html#a83f076111496c324f745edb92ca32773',1,'dev::DEV_SIMPLE_EXCEPTION(NoUPnPDevice)'],['../namespacedev.html#a28d0d371bcd53a79ec6e025388dfaf26',1,'dev::DEV_SIMPLE_EXCEPTION(NoNetworking)'],['../namespacedev.html#a3bc6cd2e673d17317a6f9ad877f33f23',1,'dev::DEV_SIMPLE_EXCEPTION(BadHexCharacter)'],['../namespacedev_1_1p2p.html#aec40319de46b05b7b29b2a9478b6d460',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRSecp256k1NotFound)'],['../namespacedev_1_1p2p.html#a85786a26bcad5c09f3a9b50a5c49010e',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRInvalidAddress)'],['../namespacedev_1_1p2p.html#a360273a2e9c9c6aaceb502cbaf914abd',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRIsTooBig)'],['../namespacedev_1_1p2p.html#a8d50f64ef8389816348aedd9ad2eb893',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRSignatureIsInvalid)'],['../namespacedev_1_1p2p.html#ac743c370e38c9d1acf4615f4ad712952',1,'dev::p2p::DEV_SIMPLE_EXCEPTION(ENRUnknownIdentityScheme)'],['../namespacetaraxa_1_1net_1_1rpc_1_1eth.html#aabff23a169ec37c540ea054fab5a53c5',1,'taraxa::net::rpc::eth::DEV_SIMPLE_EXCEPTION()']]],
  ['dev_5fsimple_5fexception_5frlp_4869',['DEV_SIMPLE_EXCEPTION_RLP',['../namespacedev.html#a7142a9d865619e40e469bf11236a2813',1,'dev::DEV_SIMPLE_EXCEPTION_RLP(BadCast)'],['../namespacedev.html#ad500bc05735dd2f89883fd0657f1897c',1,'dev::DEV_SIMPLE_EXCEPTION_RLP(UndersizeRLP)'],['../namespacedev.html#a926847180de7bc693356b6c9eea6fff0',1,'dev::DEV_SIMPLE_EXCEPTION_RLP(OversizeRLP)'],['../namespacedev.html#a0ec8a6a96f1b00a388296a2b515118c4',1,'dev::DEV_SIMPLE_EXCEPTION_RLP(BadRLP)']]],
  ['diff_4870',['diff',['../namespacedev.html#ae706766323baacdbdbe9ddb221e0cd92',1,'dev']]],
  ['difficulty_4871',['difficulty',['../group___final_chain.html#a992dc52632834c876af8ba07aa474028',1,'taraxa::final_chain::BlockHeader']]],
  ['dirnamefromfile_4872',['dirNameFromFile',['../classtaraxa_1_1cli_1_1_config.html#a34933d60f04538b88a80bccff8f95bff',1,'taraxa::cli::Config']]],
  ['disablesnapshots_4873',['disableSnapshots',['../classtaraxa_1_1_db_storage.html#a1389c4d576ca86153be5668efdff49de',1,'taraxa::DbStorage']]],
  ['disconnect_4874',['disconnect',['../structdev_1_1p2p_1_1_host.html#a2a5364b0c6d1c7fd2793f15774e084a4',1,'dev::p2p::Host::disconnect()'],['../structdev_1_1p2p_1_1_session.html#afc4ab254ddf7e74c2b814a1697ebcce2',1,'dev::p2p::Session::disconnect()'],['../structdev_1_1p2p_1_1_u_d_p_socket_face.html#ac35c32537a1f553fae7690aec96df087',1,'dev::p2p::UDPSocketFace::disconnect()'],['../classdev_1_1p2p_1_1_u_d_p_socket.html#af8ee5b2d1ec1ccb9c6e51a057b103f5b',1,'dev::p2p::UDPSocket::disconnect()'],['../classtaraxa_1_1network_1_1tarcap_1_1_packet_handler.html#a60a25f09bb4858e081bdfcde7a1361a0',1,'taraxa::network::tarcap::PacketHandler::disconnect()']]],
  ['disconnect_5f_4875',['disconnect_',['../structdev_1_1p2p_1_1_session.html#ad7df2f6c683c28fcdee77b9d695cecce',1,'dev::p2p::Session']]],
  ['disconnectwitherror_4876',['disconnectWithError',['../classdev_1_1p2p_1_1_u_d_p_socket.html#a217844e4646e15f30c5b1fe7ad8a372c',1,'dev::p2p::UDPSocket']]],
  ['discoverydatagram_4877',['DiscoveryDatagram',['../structdev_1_1p2p_1_1_discovery_datagram.html#aaf5f09106022dafba70fb4226881bf9c',1,'dev::p2p::DiscoveryDatagram::DiscoveryDatagram(bi::udp::endpoint const &amp;_to)'],['../structdev_1_1p2p_1_1_discovery_datagram.html#a896187cbb24b61ba1d23ef98da134a88',1,'dev::p2p::DiscoveryDatagram::DiscoveryDatagram(bi::udp::endpoint const &amp;_from, NodeID const &amp;_fromid, h256 const &amp;_echo)'],['../structdev_1_1p2p_1_1_ping_node.html#aaf5f09106022dafba70fb4226881bf9c',1,'dev::p2p::PingNode::DiscoveryDatagram(bi::udp::endpoint const &amp;_to)'],['../structdev_1_1p2p_1_1_ping_node.html#a896187cbb24b61ba1d23ef98da134a88',1,'dev::p2p::PingNode::DiscoveryDatagram(bi::udp::endpoint const &amp;_from, NodeID const &amp;_fromid, h256 const &amp;_echo)']]],
  ['distance_4878',['distance',['../classdev_1_1p2p_1_1_node_table.html#a2aa17544301359305d2c4fa87f7ae810',1,'dev::p2p::NodeTable']]],
  ['distribute_5frewards_4879',['distribute_rewards',['../group___final_chain.html#a5a10e868607af052a63210109ae2934f',1,'taraxa::state_api::StateAPI']]],
  ['do_5faccept_4880',['do_accept',['../classtaraxa_1_1net_1_1_ws_server.html#aad26dbf80dab4ef48a5fd9321b168dc9',1,'taraxa::net::WsServer']]],
  ['do_5fdiscov_4881',['do_discov',['../structdev_1_1p2p_1_1_host.html#aedc2485cf79df61d01fa34af8a0c03b6',1,'dev::p2p::Host']]],
  ['do_5fread_4882',['do_read',['../classtaraxa_1_1net_1_1_ws_session.html#a72434d357dad6f1b9446f46110afe6bf',1,'taraxa::net::WsSession']]],
  ['do_5fwork_4883',['do_work',['../structdev_1_1p2p_1_1_host.html#a4d01c5e35e506c1ab3a51e8fc9c58d9f',1,'dev::p2p::Host']]],
  ['dodiscovery_4884',['doDiscovery',['../classdev_1_1p2p_1_1_node_table.html#a9e312771d262e0b4e8a02a4a45fcfeb6',1,'dev::p2p::NodeTable']]],
  ['dodiscoveryround_4885',['doDiscoveryRound',['../classdev_1_1p2p_1_1_node_table.html#a16ffc37a4d57247b6a3df373722dfc08',1,'dev::p2p::NodeTable']]],
  ['doendpointtracking_4886',['doEndpointTracking',['../classdev_1_1p2p_1_1_node_table.html#a008f3077a46a7dbc8cd14cc00f76aa49',1,'dev::p2p::NodeTable']]],
  ['dohandletimeouts_4887',['doHandleTimeouts',['../classdev_1_1p2p_1_1_node_table.html#a6dea38ad360fe5a2eafbd822a0774ed0',1,'dev::p2p::NodeTable']]],
  ['doread_4888',['doRead',['../structdev_1_1p2p_1_1_session.html#ae5897e387a91a07fb6fb184dd454c391',1,'dev::p2p::Session::doRead()'],['../classdev_1_1p2p_1_1_u_d_p_socket.html#a817936d11790fed60b5ecc3b18bad60a',1,'dev::p2p::UDPSocket::doRead()']]],
  ['dowrite_4889',['doWrite',['../classdev_1_1p2p_1_1_u_d_p_socket.html#aca5a57f30d9094b0f2a3cd96ed2a48d2',1,'dev::p2p::UDPSocket']]],
  ['dpos_5feligible_5ftotal_5fvote_5fcount_4890',['dpos_eligible_total_vote_count',['../group___final_chain.html#afa0df0c96982e3fb920fae205e3ef0d9',1,'taraxa::final_chain::FinalChain::dpos_eligible_total_vote_count()'],['../group___final_chain.html#a4f67cd91d38cc3f873506b94d66ee916',1,'taraxa::state_api::StateAPI::dpos_eligible_total_vote_count()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#afd9369027b9b22c13e82d22ac9f07686',1,'taraxa::final_chain::FinalChainImpl::dpos_eligible_total_vote_count()']]],
  ['dpos_5feligible_5fvote_5fcount_4891',['dpos_eligible_vote_count',['../group___final_chain.html#a92cea442deb2f451264773571de49e06',1,'taraxa::final_chain::FinalChain::dpos_eligible_vote_count()'],['../group___final_chain.html#a954304ae0af99b51d175d9ba4a09a976',1,'taraxa::state_api::StateAPI::dpos_eligible_vote_count()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#a47a38c5931808a84a9d0d519716a43f6',1,'taraxa::final_chain::FinalChainImpl::dpos_eligible_vote_count()']]],
  ['dpos_5fget_5fvrf_5fkey_4892',['dpos_get_vrf_key',['../group___final_chain.html#ab193319e8081030abcfa62a172ab6cfd',1,'taraxa::final_chain::FinalChain::dpos_get_vrf_key()'],['../group___final_chain.html#a36c579a28c11da0a88eabf8cbf601770',1,'taraxa::state_api::StateAPI::dpos_get_vrf_key()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#a9ff1eeb365ce30d17f259213df2e72d4',1,'taraxa::final_chain::FinalChainImpl::dpos_get_vrf_key()']]],
  ['dpos_5fis_5feligible_4893',['dpos_is_eligible',['../group___final_chain.html#a218a8d86259ff424fdbda18138fb0bad',1,'taraxa::final_chain::FinalChain::dpos_is_eligible()'],['../group___final_chain.html#a87694ae315e7ad895031180ea979bd76',1,'taraxa::state_api::StateAPI::dpos_is_eligible()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#a4471baab3d2f87ef2903996fdf2dd2bd',1,'taraxa::final_chain::FinalChainImpl::dpos_is_eligible()']]],
  ['dpos_5ftotal_5famount_5fdelegated_4894',['dpos_total_amount_delegated',['../group___final_chain.html#a55221355b4db049a10316cb8f26d0da6',1,'taraxa::final_chain::FinalChain::dpos_total_amount_delegated()'],['../group___final_chain.html#aec95eaeae9fd2fb610be5092f391ff6a',1,'taraxa::state_api::StateAPI::dpos_total_amount_delegated()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#ac5c66dec9369b71a8f6e5c9c52a2aaa3',1,'taraxa::final_chain::FinalChainImpl::dpos_total_amount_delegated()']]],
  ['dpos_5ftotal_5fsupply_4895',['dpos_total_supply',['../group___final_chain.html#aeefd42d4c2ad52e63a2753ef1928d858',1,'taraxa::final_chain::FinalChain::dpos_total_supply()'],['../group___final_chain.html#a80fe2bc044b4903a67e119baf6ccfc8c',1,'taraxa::state_api::StateAPI::dpos_total_supply()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#a55f0e0af06c0bb259b660514cdfc5c05',1,'taraxa::final_chain::FinalChainImpl::dpos_total_supply()']]],
  ['dpos_5fvalidators_5ftotal_5fstakes_4896',['dpos_validators_total_stakes',['../group___final_chain.html#ad9f46da3a897b3cf463ef9d40d768614',1,'taraxa::final_chain::FinalChain::dpos_validators_total_stakes()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#ae864ae14f6ce00db133024bda8626be1',1,'taraxa::final_chain::FinalChainImpl::dpos_validators_total_stakes()'],['../group___final_chain.html#a5022eba73f454bf6a794b85c68320c74',1,'taraxa::state_api::StateAPI::dpos_validators_total_stakes()']]],
  ['dpos_5fvalidators_5fvote_5fcounts_4897',['dpos_validators_vote_counts',['../group___final_chain.html#a12311645db6c3ff34e755426250042ed',1,'taraxa::final_chain::FinalChain::dpos_validators_vote_counts()'],['../group___final_chain.html#aaeb12b5964f6d68fbd808ec1df0fbac3',1,'taraxa::state_api::StateAPI::dpos_validators_vote_counts()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#ade33f4d9d8124429bddd3ac6638f26d7',1,'taraxa::final_chain::FinalChainImpl::dpos_validators_vote_counts()']]],
  ['dpos_5fyield_4898',['dpos_yield',['../group___final_chain.html#a26219b4de9c395dc0289e4a4ee512bd7',1,'taraxa::final_chain::FinalChain::dpos_yield()'],['../group___final_chain.html#a345ad314097d6b479f6440b4364cff6b',1,'taraxa::state_api::StateAPI::dpos_yield()'],['../classtaraxa_1_1final__chain_1_1_final_chain_impl.html#aaa3c2118b09ebe56f4504965571da7a5',1,'taraxa::final_chain::FinalChainImpl::dpos_yield()']]],
  ['drawgraph_4899',['drawGraph',['../group___d_a_g.html#aa790302c9ac6ce992850dd09facdfc83',1,'taraxa::Dag::drawGraph()'],['../group___d_a_g.html#a4338078fd5288067701b3f276076c01a',1,'taraxa::DagManager::drawGraph(std::string const &amp;dotfile) const']]],
  ['drawpivotgraph_4900',['drawPivotGraph',['../group___d_a_g.html#a6358aeccb8b0ed554c321dda03a02768',1,'taraxa::DagManager']]],
  ['drawtotalgraph_4901',['drawTotalGraph',['../group___d_a_g.html#a4ab4fb411fe6447b8a556a53c1ea700c',1,'taraxa::DagManager']]],
  ['drop_4902',['drop',['../structdev_1_1p2p_1_1_session.html#a7d6f8bf80f525a3a26da89f6bd892b81',1,'dev::p2p::Session']]],
  ['dropnode_4903',['dropNode',['../classdev_1_1p2p_1_1_node_table.html#a7ab7bf299174016952f720ed9d682013',1,'dev::p2p::NodeTable']]],
  ['dry_5frun_5ftransaction_4904',['dry_run_transaction',['../group___final_chain.html#ae8310bd008463bad92c1fa31849e8d2a',1,'taraxa::state_api::StateAPI']]],
  ['duration_4905',['duration',['../classdev_1_1_timer.html#a7094444ed85065f0e8db6226f11b108a',1,'dev::Timer']]]
];
