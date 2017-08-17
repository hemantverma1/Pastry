#include <iostream>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <sstream>
#include <errno.h>
#include <fcntl.h>
#include <ctime>
#include <openssl/md5.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <bits/stdc++.h>

#define MAXBUF 8192
#define D_IP "-.-.-.-"
#define D_ID "-"
#define D_P "-"

using namespace std;

struct cell {
	string ip;
	string portNo;
	string nodeId;
};
struct StateTable
{
	string nodeId;
	string ip;
	string portNo;
	struct cell neighbourhoodSet[4];
	struct cell leafSet[4];
	struct cell routingTable[8][16];
};

//------------------------------------------------

/* comparator to compare two string representing hexadecimal numbers*/

class comparator {
	public:
	bool operator()(const struct cell& l, const struct cell& r)
	{	
		return stoll(l.nodeId,0,16) < stoll(r.nodeId,0,16);
	}  
};

bool operator==(const struct cell& lhs, const struct cell& rhs)
{
	return lhs.nodeId == rhs.nodeId;
}


//------------------------------------------------

/*function*/
string md5(string input_str);
vector<string> parse(string line, char delim=' ');
void create_node(int portNo);
string extractPublicIP (void) ;
int connect_to_server(string ip, int portNo);
void printRouteTable(StateTable);
void printCell(struct cell& c);
void printNeighbourSet(StateTable);
void printLeafSet(StateTable);
bool ifInLeafSetRange(string nodeId, struct cell& result);
struct StateTable deSerialize(string recieved);
string serialize(struct StateTable state);
struct cell route(string nodeId); 
void sendMsg(string msg, string ip, string port);
void maintainLeafSet(struct StateTable);
void copyToRoutingTable(struct StateTable);
void printvector(vector<struct cell>& v);
void shareRoutingTable(void);
void setKey(string key, string value);
void getKey(string key, string ip, string portNo);
void dump(StateTable state);

//------------------------------------------------

/* GLOBAL StateTable storing data of this node*/
StateTable state;
map<char, int> hexd;
set<int> recievedRouting;
int terminatingHop;
bool gotTerminating=false;
bool joiningComplete=false;
map<string, string> hashTable;

int main()
{
	string line;
	vector<string> command;
	hexd['0'] = 0;
	hexd['1'] = 1;
	hexd['2'] = 2;
	hexd['3'] = 3;
	hexd['4'] = 4;
	hexd['5'] = 5;
	hexd['6'] = 6;
	hexd['7'] = 7;
	hexd['8'] = 8;
	hexd['9'] = 9;
	hexd['a'] = 10;
	hexd['b'] = 11;
	hexd['c'] = 12;
	hexd['d'] = 13;
	hexd['e'] = 14;
	hexd['f'] = 15;

	while(1)
	{
		std::getline(std::cin,line);
		command = parse(line);
		if(!command.empty()) {
			if(command[0] == "port") {
				state.portNo = command[1];
				state.ip = extractPublicIP();
				state.nodeId = md5(state.ip + ":" + state.portNo).substr(0, 8);
			}
			else if(command[0] == "create") {
				thread server(create_node, stoi(state.portNo));
				server.detach();
			}
			else if(command[0] == "join") {
				string buddy_ip = command[1];
				string buddy_port = command[2];
				string msg = "GETMEJOINED#" + state.nodeId + "#" + state.ip + "#" + state.portNo;
				sendMsg(msg, buddy_ip, buddy_port);
			}
			else if(command[0] == "routetable") 
				printRouteTable(state);
			else if(command[0] == "nset")
				printNeighbourSet(state);
			else if(command[0] == "lset")
				printLeafSet(state);
			else if(command[0] == "put") {
				string key = command[1];
				string value = command[2];
				setKey(key, value);
			}
			else if(command[0] == "get")
				getKey(command[1], state.ip, state.portNo);
			else if(command[0] == "dump")
				dump(state);
		}
	}
	return 0;
}


void create_node(int portNo)
{
	int sockfd, newsockfd, client_sock_adr_len;
	struct sockaddr_in sock_adr, client_sock_adr;
	char buffer[MAXBUF], client_ip[16];
	int n_recvd, ret;
	string msg;

	/* make all entries in routing table to default*/

	for(int i=0; i<4; i++) {
		state.neighbourhoodSet[i].ip = D_IP;
		state.neighbourhoodSet[i].portNo = D_P;
		state.neighbourhoodSet[i].nodeId = D_ID;

		state.leafSet[i].ip = D_IP;
		state.leafSet[i].portNo = D_P;
		state.leafSet[i].nodeId = D_ID;
	}

	for(int i=0; i<8; i++) {
		for(int j=0; j<16; j++) {
			state.routingTable[i][j].ip = D_IP;
			state.routingTable[i][j].portNo = D_P;
			state.routingTable[i][j].nodeId = D_ID;
		}
		struct cell myOwn;
		myOwn.ip = state.ip;
		myOwn.nodeId = state.nodeId;
		myOwn.portNo = state.portNo;
		state.routingTable[i][hexd[state.nodeId[i]]] = myOwn;
	}
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) { perror("Unable to run socket");	exit(0); }
	memset(&sock_adr, 0, sizeof(sock_adr));
	memset(&client_sock_adr, 0, sizeof(client_sock_adr));
	sock_adr.sin_family = AF_INET;
	sock_adr.sin_port = htons(portNo);
	memset(&(sock_adr.sin_zero), 0, sizeof(sock_adr.sin_zero));
	sock_adr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(sockfd, (sockaddr *)&sock_adr, sizeof(sock_adr));
	if(ret < 0) { perror("Unable to bind"); exit(0); }
	ret = listen(sockfd, 5);
	if(ret == -1) { perror("Unable to listen"); exit(0); }

	while(1)
	{
		client_sock_adr_len = sizeof(client_sock_adr);
		newsockfd = accept(sockfd, (sockaddr *)&client_sock_adr, (socklen_t *)&client_sock_adr_len);
		if(newsockfd == -1) { perror("Error while accepting a connection");	exit(0); }
		memset(buffer, 0, sizeof(buffer));	

		n_recvd = recv(newsockfd, (void *)buffer, sizeof(buffer)-1, 0);
		if(n_recvd < 0) {
			msg = "No request recieved from " + string(client_ip);
			exit(0);
		}
		close(newsockfd);

		string in_buffer(buffer);
		//cout<<"Recieved msg is "<<in_buffer<<endl;

		std::size_t found = in_buffer.find_first_of('#');
		string msg_type = in_buffer.substr(0, found);
		cout<<"MSG type: "<<msg_type<<endl;
		
		if(msg_type == "GETMEJOINED")
		{
			vector<string> msg;
			msg = parse(in_buffer, '#');
			struct cell destination = route(msg[1]);
			//cout<<"Msg will be routed to "<<destination.nodeId<<endl;
			if(destination.nodeId == state.nodeId) { //if destination is me only
				string routingTable = "ROUTINGTABLE#1#1#1#"; /*msg semantics are: {ROUTINGTABLE#<buddy_flag>#<terminating_flag#<hop_number>#<routing table>} */
				routingTable += serialize(state);
				sendMsg(routingTable, msg[2], msg[3]);
				cout<<"Will not be routed\n";
			}
			else {
				/* send routing table to requesting node */
				string routingTable = "ROUTINGTABLE#1#0#1#";
				routingTable += serialize(state);
				sendMsg(routingTable, msg[2], msg[3]);
				
				/*create a JOIN message and route accordingly */
				string joinMsg = "JOIN#2#" + msg[1] + "#" + msg[2] + "#" + msg[3]; /*msg semantics are: {JOIN#<nextHopNumber to be used in ROUTINGTABLE message#nodeId,ip,port} */
				sendMsg(joinMsg, destination.ip, destination.portNo);
				//cout<<"Will be routed to "<<destination.nodeId<<endl;
			}
				
		}
		else if(msg_type == "ROUTINGTABLE")
		{
			/*int buddy_flag = in_buffer[13] - '0';*/
			int terminating_flag = in_buffer[15] - '0';
			size_t fifthHash = in_buffer.find_first_of("#", 17);
			int hopCount = stoi(in_buffer.substr(17,fifthHash-17));
			recievedRouting.insert(hopCount);
			if(terminating_flag) {
				gotTerminating=true;
				terminatingHop=hopCount;
			}
			struct StateTable recvdTable = deSerialize(in_buffer.substr(fifthHash+1));
			//cout<<"Got routing table:"<<endl;
			//printRouteTable(recvdTable);
			//cout<<"hop: "<<hopCount<<endl;

			maintainLeafSet(recvdTable);
			copyToRoutingTable(recvdTable);
			if(gotTerminating && (int)recievedRouting.size() == terminatingHop) {
				joiningComplete=true;
				cout<<"Joining complete\n";
				//cout<<"Sharing table with others\n";
				shareRoutingTable();
			}
		}
		else if(msg_type == "JOIN")
		{
			vector<string> msg = parse(in_buffer, '#');
			string hopCount = msg[1];
			struct cell destination = route(msg[2]);
			//cout<<"Msg will be routed to "<<destination.nodeId<<endl;
			if(destination.nodeId == state.nodeId) { //if destination is me only
				string routingTable = "ROUTINGTABLE#0#1#" + hopCount + "#"; /*msg semantics are: {ROUTINGTABLE#<buddy_flag>#<terminating_flag#<hop_number>#<routing table>} */
				routingTable += serialize(state);
				sendMsg(routingTable, msg[3], msg[4]);
				//cout<<"Will not be routed\n";
			}
			else {
				/* send routing table to requesting node */
				string routingTable = "ROUTINGTABLE#0#0#" + hopCount + "#";
				routingTable += serialize(state);
				sendMsg(routingTable, msg[3], msg[4]);
				
				/*create a JOIN message and route accordingly */
				int nextHop = stoi(hopCount)+1;
				string joinMsg = "JOIN#" + to_string(nextHop) + "#" + msg[2] + "#" + msg[3] + "#" + msg[4]; 
				/*msg semantics are: {JOIN#<nextHopNumber to be used in ROUTINGTABLE message#nodeId,ip,port} */
				sendMsg(joinMsg, destination.ip, destination.portNo);
				//cout<<"Will be routed to "<<destination.nodeId<<endl;
			}
		}
		else if(msg_type == "UPDATETABLE")
		{	
			//cout<<"Recieved table is\n";
			struct StateTable recvdTable = deSerialize(in_buffer.substr(12));
			//printRouteTable(recvdTable);

			maintainLeafSet(recvdTable);
			copyToRoutingTable(recvdTable);
		}
		else if(msg_type == "SETKEY")
		{	
			vector<string> msg = parse(in_buffer, '#');
			string key = msg[1];
			string value = msg[2];
			setKey(key, value);
		}
		else if(msg_type == "GETKEY")
		{
			vector<string> msg = parse(in_buffer, '#');
			getKey(msg[1], msg[2], msg[3]);
		}
		else if(msg_type == "VALUE")
		{
			vector<string> msg = parse(in_buffer, '#');
			cout<<msg[1]<<" : "<<msg[2]<<endl;
		}

		/*recieve the msg - parse the msg -

		if msg is "GET ME JOINED <nodeId, Ip, port>":
			1.Make a msg "ROUTING TABLE", put your routing table in it and set BUDDY NODE = 1 in the msg
				- send above msg to requester's ip:port
			2.Create a msg "JOIN <nodeId, Ip, port>" and route appropriately

		if msg is "JOIN <nodeId, Ip, port>" : 
			1.Find the node this msg must go to 
				- if its you only - set TERMINATING NODE = 1
				  else
				  - Route the msg appropriately
			2.Send your routing table and TERMINATING NODE to <Ip:port> - as a "ROUTING TABLE" msg

		if msg is "ROUTING TABLE <table in some serialized form>"
			- if BUDDY NODE == 1
				- copy its neighbourhood set to your own neighbourhood set
			- if TERMINATING NODE == 1
				- copy its leaf set to your leaf set
			- make your routing table according to the algorithm 

		----- When X has recieved all the routing tables and it has made its routing table - it must transmit its routing table to others to let them know of its presence
			1.X will transmit its routing table to all nodes in its leaf set/neighbour hood set/routing table - as a "UPDATE TABLE" msg
	
		if msg is "UPDATE TABLE <table in some serialized form>"
			- update your routing table according to the algorithm
			- update your leaf set or neighbourhood if needed

		if "GET a <requesting node's <ip:port>" 
			- md5(a) 
			- if this hash matches with you only other than anyone else 
				- reply as "VAL_REPLY <a, value>" to requesting node's <ip:port>
			  else 
				- route normally on hash value
		 
		if "PUT a 5" 
			- md5(a) 
			- if this hash matches with you only other than anyone else
				- save variable and value at your node only
			- else
				- route normally on hash value
		*/

	}
}



string md5(string input_str)
{
	char result[33];
	unsigned char digest[MD5_DIGEST_LENGTH]; 
	MD5((unsigned char *)(input_str.c_str()), input_str.length(), (unsigned char*)&digest);
	
	for(int i = 0; i < 16; i++)
		sprintf(&result[i*2], "%02x", (unsigned int)digest[i]);

	return string(result);
}

vector<string> parse(string line, char delim)
{
	vector<string> result;
	std::stringstream ss;
	ss.str(line);
	string token;
	while (std::getline(ss, token, delim)) 
		result.push_back(token);
	return result;
}

string extractPublicIP (void) 
{
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;
	string ret;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			if(ifa->ifa_name[0] == 'e') {
				ret = string(addressBuffer);
				if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
				return ret;
			}
        }
    }
	return ret;
}

int connect_to_server(string ip, int portNo)
{
	int sockfd, ret;
	struct sockaddr_in server_sock_adr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1)
	{
		perror("Unable to run socket in client");
		exit(0);
	}

	memset(&server_sock_adr, 0, sizeof(server_sock_adr));
	//server_details = gethostbyname("localhost");
	
	server_sock_adr.sin_family = AF_INET;
	server_sock_adr.sin_port = htons(portNo);
	memset(&(server_sock_adr.sin_zero), 0, sizeof(server_sock_adr.sin_zero));
	inet_pton(AF_INET, ip.c_str(), &(server_sock_adr.sin_addr));
	ret = connect(sockfd, (struct sockaddr *)&server_sock_adr, (socklen_t)sizeof(server_sock_adr));
	if(ret < 0)
	{
		perror("Server unreachable; unable to connect");
		return -1;
	}
	return sockfd;
}

void dump(StateTable state)
{
	cout<<state.nodeId<<"  "<<state.ip<<"  "<<state.portNo<<endl;
	printNeighbourSet(state);
	printLeafSet(state);
	for(int i=0; i<8; i++) {
		for(int j=0; j<16; j++)
			printCell(state.routingTable[i][j]);
		cout<<endl;
	}
}

void printRouteTable(StateTable state)
{
	cout<<state.nodeId<<"  "<<state.ip<<"  "<<state.portNo<<endl;
	cout<<"---------------------------------------------------------------"<<endl;
	for(int i=0; i<8; i++) {
		for(int j=0; j<16; j++)
			printCell(state.routingTable[i][j]);
		cout<<endl;
	}
}

void printCell(struct cell& c)
{
	//cout<<c.ip<<":"<<c.portNo<<"("<<c.nodeId<<")  ";
	/*size_t pos = c.ip.find_last_of(".");
	cout<<c.ip.substr(pos+1)<<":"<<c.portNo<<"("<<c.nodeId<<")  ";*/
	cout<<setfill(' ') << setw(8) <<c.nodeId<<" ";
}

void printNeighbourSet(StateTable state)
{
	for(int i=0; i<4; i++)
		printCell(state.neighbourhoodSet[i]);
	cout<<endl<<"---------------------------------------------------------------"<<endl;
}
void printLeafSet(StateTable state)
{
	for(int i=0; i<4; i++)
		printCell(state.leafSet[i]);
	cout<<endl<<"---------------------------------------------------------------"<<endl;
}

struct cell route(string nodeId) //where a given nodeId should be routed to
{
	//cout<<"Node id which came is "<<nodeId<<endl;
	struct cell result;
	if(ifInLeafSetRange(nodeId, result)) {
		//cout<<"Returned from inLeafSet succefully\n";
		return result;
	}
	else {
		//cout<<"Inside else block\n";
		int i;
		for (i=0; i<8 && state.nodeId[i]==nodeId[i]; i++)
			;
		if(state.routingTable[i][hexd[nodeId[i]]].nodeId != D_ID)
			return state.routingTable[i][hexd[nodeId[i]]];
		else {
			bool flag=false;
			long long min_diff;
			int pos;
			for(int j=0; j<16; j++) {
				if(state.routingTable[i][j].nodeId != D_ID) {
					if(!flag) { min_diff = abs(stoll(nodeId, 0, 16) - stoll(state.routingTable[i][j].nodeId, 0, 16)); pos = j; }
					else{
						long long tmp_d = abs(stoll(nodeId, 0, 16) - stoll(state.routingTable[i][j].nodeId, 0, 16));
						if(tmp_d < min_diff) { min_diff = tmp_d; pos = j; }
					}
				}			
			}
			//cout<<"Returned from route successfully\n";
			return state.routingTable[i][pos];
		}	
	}
}

bool ifInLeafSetRange(string nodeId, struct cell& result)
{
	if(state.leafSet[0].nodeId != D_ID) {	
		long long low = stoll(state.leafSet[0].nodeId, 0, 16);
		long long hi;
		for(int i=0; i<4 && state.leafSet[i].nodeId != D_ID; i++)
			hi = stoll(state.leafSet[i].nodeId, 0, 16);
		if(stoll(nodeId, 0, 16) >= low && stoll(nodeId, 0, 16) <= hi)
		{
			int pos=-1;
			long long min_diff = abs(stoll(nodeId, 0, 16) - stoll(state.nodeId,0,16));

			for(int i=0; i<4 && state.leafSet[i].nodeId != D_ID; i++) {
				long long tmp = abs(stoll(nodeId, 0, 16) - stoll(state.leafSet[i].nodeId, 0, 16));
				if(tmp<min_diff) {
					min_diff = tmp; pos=i;
				}
			}
			if(pos == -1) {//Maximum match is with current node only
				result.ip = state.ip;
				result.nodeId = state.nodeId;
				result.portNo = state.portNo;
			}
			else 
				result = state.leafSet[pos];
			return true;
		}
		else	
			return false;
	}
	else
		return false;
}

// ========================================================== SERIALIZE ==============================================//

string serialize(struct StateTable state){
	//cout<<"Serializing"<<endl;	
	string toSend="";
	toSend=state.nodeId+"#"+state.ip+"#"+state.portNo+"#";

	for(int i=0;i<4;i++){

		toSend += state.neighbourhoodSet[i].ip+"#"+state.neighbourhoodSet[i].portNo+"#"+state.neighbourhoodSet[i].nodeId+"#";
	}
	for(int i=0;i<4;i++){

		toSend += state.leafSet[i].ip+"#"+state.leafSet[i].portNo+"#"+state.leafSet[i].nodeId+"#";
	}
	for(int i=0;i<8;i++) {
		for(int j=0;j<16;j++) {

			toSend+=state.routingTable[i][j].ip+"#"+state.routingTable[i][j].portNo+"#"+state.routingTable[i][j].nodeId+"#";
		}
	}

	return toSend;
}

// =================================================== DESERIALIZE ===================================================//

struct StateTable deSerialize(string recieved){
	
	StateTable stateNew;
	
	stateNew.nodeId=recieved.substr(0,recieved.find_first_of('#'));
	recieved.erase(0,recieved.find_first_of('#')+1);

	stateNew.ip=recieved.substr(0,recieved.find_first_of('#'));
	recieved.erase(0,recieved.find_first_of('#')+1);

	stateNew.portNo=recieved.substr(0,recieved.find_first_of('#'));
	recieved.erase(0,recieved.find_first_of('#')+1);

	for(int i=0;i<4;i++){
		stateNew.neighbourhoodSet[i].ip=recieved.substr(0,recieved.find_first_of('#'));
		recieved.erase(0,recieved.find_first_of('#')+1);
		stateNew.neighbourhoodSet[i].portNo=recieved.substr(0,recieved.find_first_of('#'));
		recieved.erase(0,recieved.find_first_of('#')+1);
		stateNew.neighbourhoodSet[i].nodeId=recieved.substr(0,recieved.find_first_of('#'));
		recieved.erase(0,recieved.find_first_of('#')+1);
	}
	for(int i=0;i<4;i++){
		stateNew.leafSet[i].ip=recieved.substr(0,recieved.find_first_of('#'));
		recieved.erase(0,recieved.find_first_of('#')+1);
		stateNew.leafSet[i].portNo=recieved.substr(0,recieved.find_first_of('#'));
		recieved.erase(0,recieved.find_first_of('#')+1);
		stateNew.leafSet[i].nodeId=recieved.substr(0,recieved.find_first_of('#'));
		recieved.erase(0,recieved.find_first_of('#')+1);
	}
	for(int i=0;i<8;i++) {
		for(int j=0;j<16;j++) {
			
			stateNew.routingTable[i][j].ip=recieved.substr(0,recieved.find_first_of('#'));
			recieved.erase(0,recieved.find_first_of('#')+1);
			stateNew.routingTable[i][j].portNo=recieved.substr(0,recieved.find_first_of('#'));
			recieved.erase(0,recieved.find_first_of('#')+1);
			stateNew.routingTable[i][j].nodeId=recieved.substr(0,recieved.find_first_of('#'));
			recieved.erase(0,recieved.find_first_of('#')+1);
		}
	}
	return stateNew;
}

void sendMsg(string msg, string ip, string port)
{
	//cout<<"Sending msg: "<<msg<<endl<<" to "<<ip<<":"<<port<<endl;
	int sockfd; //, n_sent;
	char buffer[MAXBUF];
	memset(buffer, 0, sizeof(buffer));

	sockfd = connect_to_server(ip, stoi(port));
	
	if(sockfd < 0) {
		cout<<"Destination server is unreachable\n";
		return; }
	snprintf(buffer, MAXBUF, "%s", msg.c_str());
	send(sockfd, (void *)buffer, strlen(buffer), 0);
	//cout<<n_sent<<" bytes sent"<<endl;
	close(sockfd);
}

void maintainLeafSet(struct StateTable recvdTable)
{
	comparator c; //object of comparator class
	vector<struct cell> lower, higher, result;
	struct cell newNode;
	newNode.ip = recvdTable.ip;
	newNode.nodeId = recvdTable.nodeId;
	newNode.portNo = recvdTable.portNo;

	if(stoll(newNode.nodeId, 0, 16) < stoll(state.nodeId, 0, 16))
		lower.push_back(newNode);
	else
		higher.push_back(newNode);
	
	for(int i=0; i<4 && recvdTable.leafSet[i].nodeId != D_ID; i++) {
		if(recvdTable.leafSet[i].nodeId != state.nodeId) {
			if(stoll(recvdTable.leafSet[i].nodeId, 0, 16) < stoll(state.nodeId, 0, 16))
				lower.push_back(recvdTable.leafSet[i]);
			else
				higher.push_back(recvdTable.leafSet[i]);
		}
	}
		
	for(int i=0; i<4 && state.leafSet[i].nodeId != D_ID; i++) {
		if(stoll(state.leafSet[i].nodeId, 0, 16) < stoll(state.nodeId, 0, 16))
			lower.push_back(state.leafSet[i]);
		else
			higher.push_back(state.leafSet[i]);
	}

	sort(lower.begin(), lower.end(), c);
	lower.erase( unique( lower.begin(), lower.end()), lower.end() );

	//cout<<"Lower: ";
	//printvector(lower);
	sort(higher.begin(), higher.end(), c);
	higher.erase( unique( higher.begin(), higher.end()), higher.end() );
	//cout<<"Higher: ";
	//printvector(higher);
	
	int got=0;
	for(vector<struct cell>::reverse_iterator it=lower.rbegin(); it!=lower.rend() && got<2; it++) {
		result.push_back(*it);
		got++;
	}
	for(vector<struct cell>::iterator it = higher.begin(); it!=higher.end() && got<4; it++) {
		result.push_back(*it);
		got++;
	}
		
	sort(result.begin(), result.end(), c);
	result.erase( unique( result.begin(), result.end()), result.end() );
	//cout<<"Result: ";
	//printvector(result);
	for(unsigned int i=0; i<result.size(); i++)
		state.leafSet[i] = result[i];
}

void copyToRoutingTable(struct StateTable recvdTable)
{
	string newNode = recvdTable.nodeId;
	int i;
	for (i=0; i<8 && state.nodeId[i]==newNode[i]; i++)
		;
	for(int j=0; j<16; j++) {
		if((state.routingTable[i][j].nodeId == D_ID || state.routingTable[i][j].nodeId == state.nodeId) && recvdTable.routingTable[i][j].nodeId != D_ID && recvdTable.routingTable[i][j].nodeId != state.routingTable[i][j].nodeId) {
			state.routingTable[i][j] = recvdTable.routingTable[i][j];
			cout<<"Updating ["<<i<<"]["<<j<<"] to "<<recvdTable.routingTable[i][j].nodeId<<endl;
		}
	}
}

void printvector(vector<struct cell>& v)
{
	for(vector<struct cell>::iterator it = v.begin(); it!=v.end(); it++)
		printCell(*it);
	cout<<endl;
}

void shareRoutingTable(void)
{
	vector<struct cell> allNodes;
	comparator c;
	string updtMsg = "UPDATETABLE#" + serialize(state);
	for(int i=0; i<4; i++) {
		if(state.leafSet[i].nodeId != D_ID)
			allNodes.push_back(state.leafSet[i]);
		if(state.neighbourhoodSet[i].nodeId != D_ID)
			allNodes.push_back(state.neighbourhoodSet[i]);
	}
	for(int i=0; i<8; i++)
		for(int j=0; j<16; j++)
			if(state.routingTable[i][j].nodeId != D_ID && state.routingTable[i][j].nodeId != state.nodeId)
				allNodes.push_back(state.routingTable[i][j]);
	sort(allNodes.begin(), allNodes.end(), c);
	allNodes.erase( unique( allNodes.begin(), allNodes.end()), allNodes.end() );
	for(vector<struct cell>::iterator it=allNodes.begin(); it!=allNodes.end(); it++) {
		sendMsg(updtMsg, (*it).ip, (*it).portNo);
		cout<<"Sent routing table to "<<(*it).nodeId<<endl;
	}
}

void setKey(string key, string value)
{
	string keyHash = md5(key).substr(0, 8);
	cout<<"Keyhash is "<<keyHash<<endl;
	struct cell destination = route(keyHash);
	if(destination.nodeId == state.nodeId) { //if destination is me only
		cout<<"Key will be set on me only"<<endl;
		hashTable[key] = value;
	}
	else {
		cout<<"SetKey msg routed to "<<destination.nodeId<<endl; 
		string setMsg = "SETKEY#" + key + "#" + value;
		sendMsg(setMsg, destination.ip, destination.portNo);
	}
}

void getKey(string key, string ip, string portNo)
{
	string keyHash = md5(key).substr(0, 8);
	struct cell destination = route(keyHash);
	if(destination.nodeId == state.nodeId) { //if destination is me only
		cout<<"Key is present at me only"<<endl;
		if(hashTable.find(key) != hashTable.end()) {
			string valMsg  = "VALUE#" + key + "#" + hashTable[key];
			sendMsg(valMsg, ip, portNo);
		}
	}
	else {
		cout<<"getKey msg routed to "<<destination.nodeId<<endl; 
		string getMsg = "GETKEY#" + key + "#" + ip + "#" + portNo;
		sendMsg(getMsg, destination.ip, destination.portNo);
	}
}


