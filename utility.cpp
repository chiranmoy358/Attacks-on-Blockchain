#include <bits/stdc++.h>
using namespace std;

// --------------------- Unique ID Generator ---------------------
string get_uuid()
{
    static random_device dev;
    static mt19937 rng(dev());
    const char *v = "0123456789abcdef";
    uniform_int_distribution<int> dist(0, 15);

    string res;
    for (int i = 0; i < 16; i++)
    {
        res += v[dist(rng)];
        res += v[dist(rng)];
    }
    return res;
}

// --------------------- Random # Generators ---------------------
int randomUniform(int min, int max)
{
    return (rand() % (max - min + 1)) + min;
}

double randomExponential(double mean)
{
    double uniform = double(rand()) / RAND_MAX;
    double r = log(1 - uniform) * (-mean);
    return r;
}

// --------------------- Simulation Output ---------------------
void summary(vector<Node *> &miners, Attacker &attacker, int alpha)
{
    int chainLen = 1;
    Node *node = miners[0];
    BlockChain *blockchain = node->blockchain;
    Block *block = attacker.lastBlock;
    vector<int> mainChainContribution(miners.size());

    while (block->blockID != "0")
    {
        chainLen++;
        mainChainContribution[block->minerID]++;
        block = blockchain->allBlocks[block->prevBlockID];
    }

    // ---------------- Summary ----------------
    cout << "\nTotal Blocks mined:  " << blockchain->allBlocks.size() << endl;
    cout << "Main Chain Length: " << chainLen << endl;
    cout << "Total Blocks mined by attacker: " << attacker.blocksGenerated << endl;
    cout << fixed << setprecision(2) << "Attacker's Blocks in the main chain: " << mainChainContribution[0] << " (" << (mainChainContribution[0] * 100.0 / chainLen) << "%)" << endl;

    cout << "\n(Ratio 1) Adversary Blocks that got into the main chain: " << double(mainChainContribution[0]) / attacker.blocksGenerated << endl;
    cout << "(Ratio 2) Length of Main Chain vs Total Blocks Generated: " << double(chainLen) / blockchain->allBlocks.size() << endl;

    // ---------------- Fork Summary ----------------
    map<int, int> forks;
    unordered_map<string, Block *> allBlocks = blockchain->allBlocks, leaves = allBlocks;
    Block *temp = attacker.lastBlock;

    // computes leaves of the blockchain
    for (auto &it : blockchain->allBlocks)
        leaves.erase(it.second->prevBlockID);

    // delete the main chain, s.t. the fork lengths can be computed
    leaves.erase(temp->blockID);
    while (temp->blockID != "0")
    {
        temp = allBlocks[temp->prevBlockID];
        allBlocks.erase(temp->blockID);
    }

    // compute the fork lengths
    for (auto &it : leaves)
    {
        int forkLen = 0;
        Block *temp = it.second;

        while (true)
        {
            forkLen++;
            if (!allBlocks.count(temp->prevBlockID))
                break;
            temp = allBlocks[temp->prevBlockID];
        }

        forks[forkLen]++;
    }

    cout << "\nFork Summary: " << (forks.size() == 0 ? "No Forks" : "") << endl;
    for (auto &it : forks)
        cout << "Fork Length: " << it.first << ", Count: " << it.second << endl;
    cout << endl;
}

void outputGraph(Node *node, unordered_map<string, int> &stateMap)
{
    int blockID = 1, nodeID = node->id;
    BlockChain *blockchain = node->blockchain;
    unordered_map<string, int> map;
    unordered_map<string, Block *> &allBlocks = blockchain->allBlocks;

    string filepath = "Output/blockchain" + to_string(node->id) + ".gh";
    ofstream file(filepath);
    file << "digraph G{\nrankdir=\"LR\"\n";

    // giving every block a integer id (string ids are too long)
    map.insert({"0", 0});
    for (auto &it : allBlocks)
        map.insert({it.first, blockID++});

    // arrival time and block type (attacker or honest)
    for (auto &it : allBlocks)
    {
        double arrivalTime = node->blockArrivalTime[it.first];
        string color = allBlocks[it.first]->minerID == 0 ? "red" : "green";
        int xlabel = stateMap[it.first];

        if (node->id == 0)
            file << map[it.first] << " [label=\"" << arrivalTime << "\" color=" << color << " xlabel=" << xlabel << "]" << '\n';
        else
            file << map[it.first] << " [label=\"" << arrivalTime << "\" color=" << color << "]" << '\n';
    }

    // edges
    for (auto &it : allBlocks)
    {
        Block *block = it.second;
        if (block->blockID == block->prevBlockID)
            continue;
        file << map[block->prevBlockID] << " -> " << map[block->blockID] << '\n';
    }

    file << "}";
    file.close();

    string graphPath = "dot -Tpdf -Nshape=rect Output/blockchain" + to_string(node->id) + ".gh -o Output/blockchain" + to_string(node->id) + ".pdf";
    system(graphPath.c_str());
}

void minerSummary(Node *node)
{
    int chainLen = 1, myBlocks = 0, myID = node->id;
    BlockChain *blockchain = node->blockchain;
    Block *block = blockchain->lastBlock;

    string filepath = "Output/miner" + to_string(node->id) + ".csv";
    ofstream file(filepath);

    // calc chain length and the miner's block in the longest chain
    while (block->blockID != "0")
    {
        chainLen++;
        myBlocks += block->minerID == myID;
        block = blockchain->allBlocks[block->prevBlockID];
    }

    // Miner's Info
    file << "Miner ID, " << node->id << '\n';
    file << "CPU Type, " << (node->isFastCPU ? "Fast" : "Slow") << '\n';
    file << "Link Type, " << (node->isFastLink ? "Fast" : "Slow") << '\n';
    file << "Blocks Received, " << blockchain->allBlocks.size() << '\n';
    file << "Longest Chain Length, " << chainLen << '\n';
    file << "Miner's Block in the Longest Chain, " << to_string(myBlocks) + " (" + to_string(myBlocks * 100.0 / chainLen) + ")" << '\n';
    file << "Balance, " << blockchain->lastBlock->balance[node->id] << '\n';

    // Fork Summary
    unordered_map<string, Block *> allBlocks = blockchain->allBlocks, leaves = allBlocks;

    // leaves of the blockchain
    for (auto &it : blockchain->allBlocks)
        leaves.erase(it.second->prevBlockID);

    // removing the longest chain, it helps to find out the forks
    Block *temp = blockchain->lastBlock;
    leaves.erase(temp->blockID);
    while (temp->blockID != "0")
    {
        temp = allBlocks[temp->prevBlockID];
        allBlocks.erase(temp->blockID);
    }
    map<int, int> forks;

    // calc # of forks and fork lengths
    for (auto &it : leaves)
    {
        int forkLen = 0;
        Block *temp = it.second;

        while (true)
        {
            forkLen++;
            if (!allBlocks.count(temp->prevBlockID))
                break;
            temp = allBlocks[temp->prevBlockID];
        }

        forks[forkLen]++;
    }

    // Output the data to the file
    file << '\n';
    for (auto &it : forks)
        file << "Fork Length, " << it.first << " (" << it.second << ")\n";

    file << "\nBlock ID, "
         << "Previous Block ID, "
         << "Arrival Timestamp, "
         << "# of Transactions in the Block" << endl;

    for (auto &it : node->blockArrivalTime)
    {
        if (blockchain->invalidBlocks.count(it.first)) // skipping invalid blocks
            continue;
        Block *block = blockchain->allBlocks[it.first];
        file << it.first << ", " << block->prevBlockID << ", " << it.second << ", " << block->transactions.size() << '\n';
    }

    file.close();
}

// --------------------- Graph Algo ---------------------

bool checkForConnectivity(vector<Node *> &miners)
{
    bool visited[miners.size()];
    fill_n(visited, miners.size(), false);
    stack<Node *> st;
    st.push(miners[0]);
    while (!st.empty())
    {
        Node *curr = st.top();
        if (visited[curr->id])
        {
            st.pop();
            continue;
        }
        visited[curr->id] = true;
        for (int i = 0; i < curr->edges.size(); i++)
        {
            st.push(curr->edges[i]);
        }
    }
    for (int i = 0; i < miners.size(); i++)
    {
        if (!visited[i])
            return false;
    }
    return true;
}

void connect(int u, int v, vector<Node *> &miners, vector<unordered_set<int>> &neighbours)
{
    neighbours[u].insert(v);
    neighbours[v].insert(u);
    miners[u]->edges.push_back(miners[v]);
    miners[v]->edges.push_back(miners[u]);

    // setting pij
    double propDelay = randomUniform(10, 500) / 1000.0; // converting ms -> s
    miners[u]->propDelay.push_back(propDelay);
    miners[v]->propDelay.push_back(propDelay);
}

void generateGraph(vector<Node *> &miners, int zeta, int n)
{
    bool connected = false;
    int attackerConnections = zeta / 100.0 * (n - 1);

    while (!connected)
    {
        vector<unordered_set<int>> neighbours(n);

        // attacker's connections
        while (neighbours[0].size() < attackerConnections)
        {
            int r = randomUniform(1, n - 1);

            if (!neighbours[0].count(r))
                connect(0, r, miners, neighbours);
        }

        for (int i = 1; i < n; i++)
        {
            int connections = randomUniform(4, 8), counter = 0;

            while (counter < n && neighbours[i].size() < connections)
            {
                int r = randomUniform(1, n - 1);

                if (r != i && neighbours[r].size() < 8 && !neighbours[i].count(r))
                    connect(i, r, miners, neighbours);
                else
                    counter++;
            }
        }

        connected = checkForConnectivity(miners);

        if (!connected)
        {
            for (auto it : miners)
            {
                it->edges.clear();
                it->propDelay.clear();
            }
        }
    }
}