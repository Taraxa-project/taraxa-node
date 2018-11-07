/*
 * @Copyright: Taraxa.io 
 * @Author: Chia-Chun Lin 
 * @Date: 2018-11-01 15:43:56 
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2018-11-02 17:48:27
 */
 
#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include "full_node.hpp"
#include "wallet.hpp"
#include "rocks_db.hpp"
#include "tcp_server.hpp"
#include "tcp_client.hpp"

using std::string;
using std::to_string;

void FullNode::createAccount(unsigned idx, unsigned long init_balance){
	UserAccount account(idx, init_balance);
	db_accounts_->put(std::to_string(idx), account.getJson());
	if (verbose_){
		cout<<"An account is created "<<idx<<endl;
		UserAccount acc(db_accounts_->get(std::to_string(idx)));
		cout<<acc;
	}
}

void FullNode::sendBlock(unsigned from, unsigned to, unsigned long new_balance){
	string send_acc = db_accounts_->get(std::to_string(from));
	string recv_acc = db_accounts_->get(std::to_string(to));
	if (send_acc.empty()){
		std::cout<<"`From` account not found "<< from <<std::endl;
		return;
	}
	if (recv_acc.empty()){
		std::cout<<"`To` account not found "<< to <<std::endl;
		return;
	}

	UserAccount sender(send_acc);
	UserAccount receiver(recv_acc);
	if (sender.getBalance()<new_balance){
		std::cout<<"Cannot send, sender balance "<<sender.getBalance()<<" is less than "<<new_balance<<std::endl;
		return;
	}
	string send_msg = sender.sendBlock(receiver.getPubKey(), new_balance);
	// update sender account (last hash updated)
	db_accounts_->put(to_string(sender.getSeed()), sender.getJson());
	
	// update send block
	rapidjson::Document document;
	document.Parse(send_msg.c_str());
	string hash=document["hash"].GetString();
	db_blocks_->put(hash, send_msg);
	
	// broadcast
	for (const auto &remote:remotes_){
		tcp_client_.connect(remote.first, remote.second);
		tcp_client_.sendMessage(send_msg);
		tcp_client_.close();
	}
	if (verbose_){
		cout<<"Block sent from "<<from<<" to "<<to<<endl;
		cout<<"Sender public key  : " << db_accounts_->get(to_string(from))<<endl;
		cout<<"Receiver public key: " << db_accounts_->get(to_string(to))<<endl;
	}
}

int main(int argc, char *argv[]){
	if (argc!=6){
		std::cout<<"Please enter host ip, port, path of account db, path of block db, other port"<<std::endl;
		return 0;
	}
	string ip=argv[1];
	unsigned port = std::stoi(string(argv[2]));
	string db_accounts_path = argv[3];
	string db_blocks_path = argv[4];
	unsigned other_port = std::stoi(string(argv[5]));
	FullNode fnode(ip, port, db_accounts_path, db_blocks_path);
	fnode.setVerbose(true);
	fnode.addRemotes(ip, other_port);
	cout<<"Full node is set"<<endl;
	string action;
	unsigned from, to;
	while (true){
		cout<<"Action [c=create account, s=send, e=exit]"<<endl;
		cin>>action>>from>>to;
		if (action == "c"){
			// from: idx, to: init balance
			fnode.createAccount(from, to);
		} else if (action == "s"){
			fnode.sendBlock(from, to, 1);
		} 
		else if (action == "e"){
			break;
		}	
	}
	return 1;
}