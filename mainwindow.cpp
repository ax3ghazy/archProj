#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <string>
#include <utility>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>
#include <regex>
#include <climits>
#include <list>
using namespace std;

#define ii pair<int, int>
#define OPTIONALSPACE "\\s*"
#define SPACE "\\s+"
#define REGFIELD "([0-9]|1[0-5])"
#define LABELFIELD "(\\w+)"
#define IMMFIELD "([-|+]?\\d+)"
//COMMENT, COMMENT add after each instruction, isComment

#define transferDepData(i,prev) Data[matchStatus] = fetchedInsts[prev].Data[WBData];

unsigned int pc, cyc;
unsigned int rs, rt, rd;
int reg[16] = { 0 }, dmem[32] = { 0 };
int s = 0;

short imm;
string func, label;
string program;
bool highlightR, highlightM;
int highlightRi, highlightMi;

map<string, int> labelTable;
vector<string> encounteredLabels;

enum predState { never, rare, often, always };
enum stage_t { IF, IS, D, ALU, M1, M2, M3, WB, X };
enum matchType { noMatch, rsMatch, rtMatch };
enum DType {WBData, RSData, RTData};
const string stages[] = { " ", "IF", "IS", "D", "ALU", "M1", "M2", "M3", "WB", "X" };
int execTable[1000][1000];
bool calcDelay[1000] = { false };
map <string, ii> delayTable;
list<int> Stack;

struct instruction {
    string func1, label1;
    unsigned int rs1, rd1, rt1;
    int Data[3]; //int WBData, RSData, RTData;
    short imm1;

    void set() {
        func1 = func;
        rs1 = rs;
        rt1 = rt;
        rd1 = rd;
        imm1 = imm;
        label1 = label;
    }
    bool operator==(const instruction  &i) const {
        return ((func1 == i.func1) && (label1 == i.label1) && (rs1 == i.rs1) && (rd1 == i.rd1) && (rt1 == i.rt1) && (imm1 == i.imm1));
    }

    void exec(){
        if (rd1 == 0)
            Data[WBData] = 0;
        else if (func == "lw")
            Data[WBData] = dmem[imm1+Data[RSData]];
        else if (func == "add")
            Data[WBData] = Data[RSData] + Data[RTData];
        else if (func == "addi")
            Data[WBData] = Data[RSData] + imm;
        else if (func == "xor")
            Data[WBData] = Data[RSData] ^ Data[RTData];
        else if (func == "slt")
            Data[WBData] = Data[RSData] < Data[RTData];
    }


};

struct BTBentry {
    int targetPC;
    int state;
};



map <int, BTBentry> BTB;
vector <instruction> imem, fetchedInsts;

bool isLabel(const string& line);
bool isJ(const string& line);
bool isBLE(const string& line);
bool isR(const string& line);
bool isJR(const string& line);
bool isI(const string& line);

bool processStream(istream& in);
bool processLine(const string& line);

int matchRegs(int i, int prev);
void adjustSWDelay (int i);
bool depStall(int i);

bool isUniqueStage(int cyc, int stage);
int stageOf(int i, int clk);
int readStage(int i);
int writeStage(int i);
void readOperands (int i);

int indextoPC(int i);
bool branchTaken(int brPC);
bool foundinBTB(int brPC = pc);
void insertinBTB();
bool modifyBTB(int brPC);
void handlePC();
void strengthenState(int& curr);
void weakenState(int& curr);
void flush(int i);
void handleJ(int i);
void handleJR(int i);
void handleJAL(int i);
void handleReturn(int i);
void simulateCycle();


void init();

bool promptNext();

#define colCnt(tw) (ui->tw->columnCount())
#define rowCnt(tw) (ui->tw->rowCount())

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ifstream fin;
    fin.open("/home/xrexeon/arch/in");
    if (!processStream(fin)) {
        //system("pause");
        //return -1; // boolean flag
    }
    init();

    ui->textEdit->setText(QString::fromStdString(program));

    ui->tableWidget->horizontalHeader()->setSectionResizeMode (QHeaderView::Fixed);
    ui->tableWidget->verticalHeader()->setSectionResizeMode (QHeaderView::Fixed);
    QTableWidgetItem *item = new QTableWidgetItem;

    item->setText("Registers");
    ui->tableWidget_2->setHorizontalHeaderItem(0, item);
    for (int i = 0; i < 32; i++){
        item = new QTableWidgetItem;
        item->setText(QString::number(i));
        ui->tableWidget_2->setVerticalHeaderItem(i, item);
    }

    QTableWidgetItem *item2 = new QTableWidgetItem;
    item2->setText("Memory");
    ui->tableWidget_3->setHorizontalHeaderItem(0, item2);
    for (int i = 0; i < 32; i++){
        item2 = new QTableWidgetItem;
        item2->setText(QString::number(i));
        ui->tableWidget_3->setVerticalHeaderItem(i, item2);
    }

    item2 = new QTableWidgetItem;
    item2->setText("PC");
    ui->tableWidget_4->setHorizontalHeaderItem(0, item2);

        QObject::connect(ui->pushButton, &QPushButton::clicked,  [=] () {



            ui->tableWidget->insertColumn(colCnt(tableWidget));
            ui->tableWidget->insertRow(rowCnt(tableWidget));

            ui->tableWidget_4->insertRow(rowCnt(tableWidget_4));
            QTableWidgetItem *item3 = new QTableWidgetItem;
            item3->setText(QString::number(pc));
            ui->tableWidget_4->setItem(rowCnt(tableWidget_4)-1, 0, item3);
            ui->tableWidget_4->selectRow(rowCnt(tableWidget_4)-1);
            ui->tableWidget_4->scrollToItem(item3);


            highlightM = highlightR = false;

            simulateCycle();



            for (int i = 0; i < rowCnt(tableWidget); i++){
                QTableWidgetItem *item = new QTableWidgetItem;
                QString text(QString::fromStdString(stages[execTable[i][cyc]+1]));
                cout << stages[execTable[i][cyc]+1] << '\n';
                item->setText(text);
                item->setFlags(item->flags()^Qt::ItemIsEditable);
                ui->tableWidget->setItem(i, colCnt(tableWidget)-1,item);
                ui->tableWidget->scrollToItem(item);
            }
            ui->tableWidget->selectColumn(cyc-1);

            for (int i = 0; i < 32; i++){
                QTableWidgetItem *item = new QTableWidgetItem;
                QString text(QString::number(reg[i]));
                cout << "reg : " << reg[i] << '\n';
                item->setText(text);
                item->setFlags(item->flags()^Qt::ItemIsEditable);
                ui->tableWidget_2->setItem(i, 0,item);



            }


            for (int i = 0; i < 32; i++){
                //ui->tableWidget->selectRow(0);
                QTableWidgetItem *item = new QTableWidgetItem;
                QString text(QString::number(dmem[i]));
                cout << "reg : " << dmem[i] << '\n';
                item->setText(text);
                item->setFlags(item->flags()^Qt::ItemIsEditable);
                ui->tableWidget_3->setItem(i, 0,item);


            }

            if (highlightR)
                ui->tableWidget_2->selectRow(highlightRi);
            else if (highlightM)
                ui->tableWidget_3->selectRow(highlightMi);

            cyc++;
    }
    );

}

MainWindow::~MainWindow()
{
    delete ui;
}


void simulateCycle() {
    /*cout << "reg 1 = " << reg[1] << '\n';
    cout << "reg 2 = " << reg[2] << '\n';
    cout << "reg 3 = " << reg[1] << '\n';*/
    int fsize = fetchedInsts.size();
    bool delayExist = false;
    for (int i = s; i < fsize; i++) {
        int prevState = execTable[i][cyc - 1];

        //fields
        rs = fetchedInsts[i].rs1, rt = fetchedInsts[i].rt1, rd = fetchedInsts[i].rd1;
        imm = fetchedInsts[i].imm1;
        func = fetchedInsts[i].func1, label = fetchedInsts[i].label1;
        int *Data = fetchedInsts[i].Data;



        // check conditions for next stage
        if (func == "sw")
            adjustSWDelay(i);

        if (depStall(i))
            delayExist = true;

        if (modifyBTB(i))
            flush(i);

        handleJ(i);
        handleJR(i);
        handleJAL(i);
        handleReturn(i);

        int state = prevState + 1;
        if (state > 8) state = -1; //why 8?
        if (delayExist)
            state = prevState;

        //we only get to readStage when no more stalls are needed; i.e., data is ready
        if (state == readStage(i)) {
            readOperands(i);
            cout << func << " : " << Data[RSData] << " " << Data[RTData] << '\n';;
        }

        //if WriteAvailable stage and it's an instruction that writes in regs
        if (state == writeStage(i) && writeStage(i) != -1) {
            fetchedInsts[i].exec();
            cout << func << " has now " << Data[WBData] << '\n';
        }

        //update regs/dmem
        if (state == WB && (func == "sw" || writeStage(i) != -1)){
            if (func == "sw"){
                int addr = imm + Data[RTData];
                if (addr < sizeof(dmem)/sizeof(int))
                    dmem[addr] = Data[RSData];
                else
                    cout << "Dmem size error -> " << i << '\n';

                highlightM = true;
                highlightMi = addr;
            } else{
                reg[rd] = Data[WBData];
                highlightR = true;
                highlightRi = rd;
            }
        }

        if (execTable[i][cyc] != X)
            execTable[i][cyc] = state;
    }

    //IF
    if (isUniqueStage(cyc, IF) && pc < imem.size()) {
        fetchedInsts.push_back(imem[pc]);
        execTable[fsize][cyc] = IF;


        if (fetchedInsts[fetchedInsts.size() - 1].func1 == "ble")
        {
            if (!foundinBTB()) {
                insertinBTB();
            }

        }


        handlePC(); //branching logic, eb3t 7aga b2a
    }

    cout << "PC: " << pc << endl;


    if (execTable[s][cyc] == WB || execTable[s][cyc] == X) s++; //????



}


void readOperands(int i) {

    bool rsdep = false, rtdep = false;
    string func = fetchedInsts[i].func1;
    int rs = fetchedInsts[i].rs1, rt = fetchedInsts[i].rt1;
    int *Data = fetchedInsts[i].Data;

    for (int prev = i - 3; prev < i; prev++) {
        if (prev < 0)
            continue;
        int matchStatus = matchRegs(i, prev);
        bool swDep = (func == "sw") ? (matchStatus == rsMatch ? readStage(i) == M1
            : readStage(i) == ALU)
            : true;
        if (matchStatus != noMatch && swDep && stageOf(prev, cyc) != X) {
            if (matchStatus == rtMatch) {
                rtdep = true;
                transferDepData(i, prev);
                if (func == "add")
                    cout << "rt dep detected\n";
            }
            if (rt == rs)
                matchStatus = rsMatch;
            if (matchStatus == rsMatch) {
                rsdep = true;
                transferDepData(i, prev);
                if (func == "add")
                    cout << "rs dep detected\n";
            }
        }
    }

    if (rtdep == false && rt != -1) //lw ?
        Data[RTData] = reg[rt];

    if (rsdep == false && rs != -1)
        Data[RSData] = reg[rs];
}

int indextoPC(int i)
{
    for (int j = 0; j < imem.size(); j++)
        if (imem[j] == fetchedInsts[i]) return j;
}

bool foundinBTB(int brPC)
{
    return(BTB.count(brPC) > 0);
}

void insertinBTB()
{
    BTBentry newBR;
    newBR.state = rare;
    newBR.targetPC = labelTable[imem[pc].label1];
    BTB[pc] = newBR;
}

bool modifyBTB(int i)
{
    int brPC = indextoPC(i);
    if (fetchedInsts[i].func1 == "ble") //cout << "branch stage " << stages[execTable[brPC][cyc - 1] + 2]  << " found? -> " << foundinBTB(brPC) << " \n";
        if (execTable[i][cyc - 1] + 1 == M1 && foundinBTB(brPC)) {
            if (branchTaken(i)) {
                if (BTB[brPC].state == always || BTB[brPC].state == often) {
                    strengthenState(BTB[brPC].state);
                    return false;
                }
                if (BTB[brPC].state == rare || BTB[brPC].state == never) {
                    strengthenState(BTB[brPC].state);
                    return true;
                }
            }
            else {
                if (BTB[brPC].state == always || BTB[brPC].state == often) {
                    weakenState(BTB[brPC].state);
                    return true;
                }
                if (BTB[brPC].state == rare || BTB[brPC].state == never) {
                    weakenState(BTB[brPC].state);
                    return false;
                }
            }
        }
    return false;
}

void strengthenState(int& curr)
{
    curr = curr < 3 ? (curr + 1) : 3;
}

void weakenState(int& curr)
{
    curr = curr > 0 ? (curr - 1) : 0;
}

bool branchTaken(int brPC)
{
    bool ret = (fetchedInsts[brPC].func1 == "ble" ? fetchedInsts[brPC].Data[RSData] <= fetchedInsts[brPC].Data[RTData] : true);
    /*cout << "taken? -> " << ret << '\n';
    cout << "brPC: " << brPC << endl;
    cout << fetchedInsts[brPC].Data[RSData] << " <= " << fetchedInsts[brPC].Data[RTData] << endl;*/
    return ret;
}

void handlePC()
{
    if (foundinBTB())
    {
        if (BTB[pc].state == always || BTB[pc].state == often) // predicted taken
            pc = BTB[pc].targetPC;
        else
            pc++;
    }
    else pc++;
}

void handleJ(int i)
{
    int jPC = indextoPC(i);
    if (fetchedInsts[i].func1 == "j" && execTable[i][cyc - 1] + 1 == ALU)
    {
        //??????
        if (pc == jPC + 2) {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            for (int j = cyc; j < cyc + 6; j++)
                execTable[i + 1][j] = X;
        }
        else if (pc == jPC + 3) {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
            for (int j = cyc; j < cyc + 6; j++)
                execTable[i + 1][j] = X;
            for (int j = cyc; j < cyc + 7; j++)
                execTable[i + 2][j] = X;
        }

        pc = labelTable[fetchedInsts[i].label1];
    }

}

void handleJR(int i)
{
    int jrPC = indextoPC(i);
    if (fetchedInsts[i].func1 == "jr" && execTable[i][cyc - 1] + 1 == ALU)
    {
        //?????
        if (pc == jrPC + 2) {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            for (int j = cyc; j < cyc + 6; j++)
                execTable[i + 1][j] = X;
        }
        else if (pc == jrPC + 3) {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
            for (int j = cyc; j < cyc + 6; j++)
                execTable[i + 1][j] = X;
            for (int j = cyc; j < cyc + 7; j++)
                execTable[i + 2][j] = X;
        }
        pc = fetchedInsts[i].Data[RSData];
    }
}

void handleJAL(int i)
{
    int jalPC = indextoPC(i);
    if (fetchedInsts[i].func1 == "jal"&& execTable[i][cyc - 1] + 1 == ALU)
    {
        if (pc == jalPC + 2) {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            for (int j = cyc; j < cyc + 6; j++)
                execTable[i + 1][j] = X;
        }
        else if (pc == jalPC + 3) {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
            for (int j = cyc; j < cyc + 6; j++)
                execTable[i + 1][j] = X;
            for (int j = cyc; j < cyc + 7; j++)
                execTable[i + 2][j] = X;
        }

        pc = labelTable[fetchedInsts[i].label1];

        Stack.push_back(jalPC + 1);

        if (Stack.size() > 4) Stack.pop_front();
    }

}

void handleReturn(int i)		//Take care
{
    int rePC = indextoPC(i);
    //if(fetchedInsts[i].func1 == "re")
    if (fetchedInsts[i].func1 == "re" && execTable[i][cyc - 1] + 1 == ALU)
    {
        if (Stack.empty());
        else
        {
            if (pc == rePC + 2) {
                cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
                for (int j = cyc; j < cyc + 6; j++)
                    execTable[i + 1][j] = X;
            }
            else if (pc == rePC + 3) {
                cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
                cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
                for (int j = cyc; j < cyc + 6; j++)
                    execTable[i + 1][j] = X;
                for (int j = cyc; j < cyc + 7; j++)
                    execTable[i + 2][j] = X;
            }


            pc = Stack.back();
            Stack.pop_back();
        }
    }
}

void flush(int i)
{
    int brPC = indextoPC(i);

    //cout << "state: " << BTB[brPC].state << endl;
    //cout << "curr pc" << pc << endl;
    //cout << "brPC " << brPC << endl;
    //cout << "target " << BTB[brPC].targetPC << endl;
    // if the branch should have been taken
    /*if (branchTaken(brPC))
    {
    if (pc - 1 != brPC)
    {
    if (pc - 1 == brPC + 1) {
    cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
    for (int j = cyc; j < cyc + 5; j++)
    execTable[i + 1][j] = X;
    }
    else if (pc - 1 == brPC + 2) {
    cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
    cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
    for (int j = cyc; j < cyc + 5; j++)
    execTable[i + 1][j] = X;
    for (int j = cyc; j < cyc + 6; j++)
    execTable[i + 2][j] = X;
    }
    else if (pc - 1 == brPC + 3 || pc - 1 == brPC + 4)
    {
    cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
    cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
    cout << "hflush 34an ma 5dto4 aho  (brPC + 3) \n" << endl;
    for (int j = cyc; j < cyc + 5; j++)
    execTable[i + 1][j] = X;
    for (int j = cyc; j < cyc + 6; j++)
    execTable[i + 2][j] = X;
    for (int j = cyc; j < cyc + 7; j++)
    execTable[i + 3][j] = X;
    }
    }
    pc = BTB[brPC].targetPC;
    }		*/

    if (branchTaken(i))
    {
        if (pc == brPC + 2) {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            for (int j = cyc; j < cyc + 5; j++)
                execTable[i + 1][j] = X;
        }
        else if (pc == brPC + 3) {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
            for (int j = cyc; j < cyc + 5; j++)
                execTable[i + 1][j] = X;
            for (int j = cyc; j < cyc + 6; j++)
                execTable[i + 2][j] = X;
        }
        else
        {
            cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
            cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
            cout << "hflush 34an ma 5dto4 aho  (brPC + 3) \n" << endl;
            for (int j = cyc; j < cyc + 5; j++)
                execTable[i + 1][j] = X;
            for (int j = cyc; j < cyc + 6; j++)
                execTable[i + 2][j] = X;
            for (int j = cyc; j < cyc + 7; j++)
                execTable[i + 3][j] = X;
        }
        pc = BTB[brPC].targetPC;
    }






    /*	if (pc >= brPC + 1) {
    cout << "hflush 34an ma 5dto4 aho (brPC + 1) \n" << endl;
    for (int j = cyc; j < cyc + 5; j++)
    execTable[i + 1][j] = X;
    if (pc > brPC + 2) {
    cout << "hflush 34an ma 5dto4 aho  (brPC + 2) \n" << endl;
    for (int j = cyc; j < cyc + 6; j++)
    execTable[i + 2][j] = X;
    if (pc > brPC + 3) {
    cout << "hflush 34an ma 5dto4 aho  (brPC + 3) \n" << endl;
    for (int j = cyc; j < cyc + 7; j++)
    execTable[i + 3][j] = X;
    }
    }
    }*/

    else if (!branchTaken(i))
    {
        /*if (pc == BTB[brPC].targetPC+1) {
        cout << "hflush 34an 5dto aho (target) \n" << endl;
        for (int j = cyc; j < cyc + 5; j++)
        execTable[i + 1][j] = X;
        }

        else if (pc == BTB[brPC].targetPC + 2) {
        cout << "hflush 34an 5dto aho (target) \n" << endl;
        cout << "hflush 34an 5dto aho (target+1) \n" << endl;

        for (int j = cyc; j < cyc + 5; j++)
        execTable[i + 1][j] = X;
        for (int j = cyc; j < cyc + 6; j++)
        execTable[i + 2][j] = X;
        }

        else if ( pc == BTB[brPC].targetPC + 3) {
        cout << "hflush 34an 5dto aho (target) \n" << endl;
        cout << "hflush 34an 5dto aho (target+1) \n" << endl;
        cout << "hflush 34an 5dto aho (target+2) \n" << endl;
        for (int j = cyc; j < cyc + 5; j++)
        execTable[i + 1][j] = X;
        for (int j = cyc; j < cyc + 6; j++)
        execTable[i + 2][j] = X;
        for (int j = cyc; j < cyc + 7; j++)
        execTable[i + 3][j] = X;
        }*/
        cout << "hflush 34an 5dto aho (target) \n" << endl;


        for (int j = cyc; j < cyc + 5; j++)
            execTable[i + 1][j] = X;
        for (int j = cyc; j < cyc + 6; j++)
            execTable[i + 2][j] = X;
        for (int j = cyc; j < cyc + 7; j++)
            execTable[i + 3][j] = X;



        pc = brPC + 1;

    }

    /*if (pc >= BTB[brPC].targetPC) {

    cout << "hflush 34an 5dto aho (target) \n" << endl;
    for (int j = cyc; j < cyc + 5; j++)
    execTable[i+1][j] = X;
    cout << "hflush 34an 5dto aho (target + 1) \n" << endl;
    if (pc > BTB[brPC].targetPC) {
    for (int j = cyc; j < cyc + 6; j++)
    execTable[i + 2][j] = X;
    if (pc > BTB[brPC].targetPC + 1) {
    cout << "hflush 34an 5dto aho (target + 2) \n" << endl;
    for (int j = cyc; j < cyc + 7; j++)
    execTable[i + 3][j] = X;
    }
    }
    }*/
}




int stageOf(int i, int clk) {
    return execTable[i][clk];
}

int matchRegs(int i, int prev) {
    if (prev >= 0 && fetchedInsts[prev].rd1 != -1) {
        if (fetchedInsts[i].func1 == "lw")
            cout << fetchedInsts[prev].rd1 << " on " << fetchedInsts[prev].func1 << " and " << fetchedInsts[i].rt1 << '\n';
        if (fetchedInsts[prev].rd1 == fetchedInsts[i].rt1)
            return rtMatch;

        if (fetchedInsts[prev].rd1 == fetchedInsts[i].rs1)
            return rsMatch;
    }
    return noMatch;
}

void adjustSWDelay(int i) {
    if (stageOf(i, cyc - 1) + 1 <= ALU)
        delayTable["sw"] = ii(-1, ALU);
    else
        delayTable["sw"] = ii(-1, M1);
}

int readStage(int i) {

    return delayTable[fetchedInsts[i].func1].second;
}

int writeStage(int i) {
    return delayTable[fetchedInsts[i].func1].first;
}

//restore this
bool depStall(int i) {
    for (int prev = i - 1; prev >= i - 3 && prev >= 0; prev--)
    {
        int matchStatus = matchRegs(i, prev);

        bool swDep = (fetchedInsts[i].func1 == "sw") ? (matchStatus == rsMatch ? readStage(i) == M1
            : readStage(i) == ALU)
            : true;

        if (stageOf(prev, cyc) != -1 && matchStatus != noMatch && swDep) {
            if (stageOf(prev, cyc)  < writeStage(prev) && stageOf(i, cyc - 1) + 1 == readStage(i))
                return true;
        }
    }
    return false;
}


//prompts the user whether to terminate the program, or advance to the next cycle
//can be easily remapped to the GUI
bool promptNext() {
    cout << "Continue? (0: no) ";
    int ans; cin >> ans;
    return ans != 0;
}

//checks if some stage (IF, IS,...) is already used by the
//previous instruction and so needs to be delayed for the current one
bool isUniqueStage(int cyc, int stage) {
    if (fetchedInsts.size() > 1) {
        return execTable[fetchedInsts.size() - 1][cyc] != stage;
    }
    else
        return true;
}

void init() {
    //WriteAvailable, ReadNeed
    //-1 -> N/A
    //edit
    delayTable["xor"] = ii(M1, ALU);
    delayTable["add"] = ii(M1, ALU);
    delayTable["slt"] = ii(M1, ALU);
    delayTable["addi"] = ii(M1, ALU);

    delayTable["jr"] = ii(-1, D);
    delayTable["ble"] = ii(-1, D);
    delayTable["j"] = ii(-1, -1);

    delayTable["jal"] = ii(-1, -1);
    delayTable["re"] = ii(-1, -1);

    delayTable["lw"] = ii(M3, ALU);
    delayTable["sw"] = ii(-1, ALU); //sw-lw at the same address???

    pc = 0;
    cyc = 1;
    memset(execTable, -1, sizeof(execTable));
    memset(reg, 0, sizeof(reg));
    memset(dmem, 0, sizeof(dmem));
}

bool isLabel(const string& line) {
    regex labell("^" OPTIONALSPACE LABELFIELD  OPTIONALSPACE ":" OPTIONALSPACE "$");
    smatch match;
    if (regex_search(line.begin(), line.end(), match, labell)) {
        //out << "this is a label named " << match[1] << ' ';
        label = match[1];
        return true;
    }
    else
        return false;
}

bool isJ(const string& line) {
    regex j("^" OPTIONALSPACE "[jJ]" SPACE LABELFIELD OPTIONALSPACE "$");
    smatch match;
    if (regex_search(line.begin(), line.end(), match, j)) {
        //cout << "this is a jump to " << match[1] << ' ';
        func = "j"; label = match[1];
        encounteredLabels.push_back(label);
        return true;
    }
    else
        return false;
}

bool isBLE(const string& line) {
    regex ble("^" OPTIONALSPACE "[bB][lL][eE]" SPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE LABELFIELD OPTIONALSPACE "$");
    smatch match;
    if (regex_search(line.begin(), line.end(), match, ble)) {
        //cout << "this is a ble between " << match[1] << " and " << match[2] << " leading to " << match[3] << ' ';
        func = "ble"; rs = stoi(match[1]); rt = stoi(match[2]); label = match[3];
        encounteredLabels.push_back(label);
        return true;
    }
    else
        return false;

}

bool isR(const string& line) {
    regex add("^" OPTIONALSPACE "[aA][dD][dD]" SPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE REGFIELD OPTIONALSPACE "$");
    regex xorr("^" OPTIONALSPACE "[xX][oO][rR]" SPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE REGFIELD OPTIONALSPACE "$");
    regex slt("^" OPTIONALSPACE "[sS][lL][tT]" SPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE REGFIELD OPTIONALSPACE "$");
    smatch match;
    if (regex_search(line.begin(), line.end(), match, add)) {
        //	cout << "this is an add between " << match[2] << " and " << match[3] << " stored in " << match[1] << ' ';
        func = "add"; rd = stoi(match[1]); rs = stoi(match[2]); rt = stoi(match[3]);
        return true;
    }
    else if (regex_search(line.begin(), line.end(), match, xorr)) {
        //cout << "this is an xor between" << match[2] << " and " << match[3] << " stored in " << match[1] << ' ';
        func = "xor"; rd = stoi(match[1]); rs = stoi(match[2]); rt = stoi(match[3]);
        return true;
    }
    else if (regex_search(line.begin(), line.end(), match, slt)) {
        //cout << "this is a slt between" << match[2] << " and " << match[3] << " stored in " << match[1] << ' ';
        func = "slt"; rd = stoi(match[1]); rs = stoi(match[2]); rt = stoi(match[3]);
        return true;
    }
    else
        return false;
}

bool isJR(const string& line) {
    regex jr("^" OPTIONALSPACE "[jJ][rR]" SPACE REGFIELD OPTIONALSPACE "$");
    smatch match;
    if (regex_search(line.begin(), line.end(), match, jr)) {
        //cout << "this is a jr to the address stored in " << match[1] << ' ';
        func = "jr"; rs = stoi(match[1]);
        return true;
    }
    else
        return false;
}

bool isJal(const string& line) {
    regex jal("^" OPTIONALSPACE "[jJ][uU][mM][pP][_][pP][rR][oO][cC][eE][dD][uU][rR][eE]" SPACE LABELFIELD OPTIONALSPACE "$");
    smatch match;
    if (regex_search(line.begin(), line.end(), match, jal)) {
        //cout << "this is a jump to " << match[1] << ' ';
        func = "jal"; label = match[1];
        encounteredLabels.push_back(label);
        return true;
    }
    else
        return false;
}

bool isRe(const string& line) {
    regex re("^" OPTIONALSPACE "[rR][eE][tT][uU][rR][nN][_][pP][rR][oO][cC][eE][dD][uU][rR][eE]" OPTIONALSPACE "$");
    if (regex_search(line.begin(), line.end(), re)) {
        //cout << "this is a jr to the address stored in " << match[1] << ' ';
        func = "re";
        return true;
    }
    else
        return false;
}
bool isI(const string& line) {
    regex addi("^" OPTIONALSPACE "[aA][dD][dD][iI]" SPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE IMMFIELD OPTIONALSPACE "$");
    regex lw("^" OPTIONALSPACE "[lL][wW]" SPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE IMMFIELD OPTIONALSPACE "[(]" OPTIONALSPACE REGFIELD OPTIONALSPACE "[)]" OPTIONALSPACE "$");
    regex sw("^" OPTIONALSPACE "[sS][wW]" SPACE REGFIELD OPTIONALSPACE "," OPTIONALSPACE IMMFIELD OPTIONALSPACE "[(]" OPTIONALSPACE REGFIELD OPTIONALSPACE "[)]" OPTIONALSPACE "$");

    smatch match;
    if (regex_search(line.begin(), line.end(), match, addi)) {
        //cout << "this is an addi between " << match[2] << " and " << match[3] << " stored in " << match[1] << ' ';
        func = "addi"; rd = stoi(match[1]); rs = stoi(match[2]); imm = stoi(match[3]);
        return true;
    }
    else if (regex_search(line.begin(), line.end(), match, lw)) {
        //cout << "this is an addi between " << match[2] << " and " << match[3] << " stored in " << match[1] << ' ';
        func = "lw"; rd = stoi(match[1]); rs = stoi(match[3]); imm = stoi(match[2]);
        return true;
    }
    else if (regex_search(line.begin(), line.end(), match, sw)) {
        //cout << "this is an addi between " << match[2] << " and " << match[3] << " stored in " << match[1] << ' ';
        func = "sw"; rt = stoi(match[3]); rs = stoi(match[1]); imm = stoi(match[2]);
        return true;
    }

    else
        return false;
}




/*


bool ModifyBTB(int i, bool taken)//return true: Prediction changed |||| False: Prediction didn't change
{
int Ptarget;
bool Pstate = CheckBTB(i,Ptarget);
if(taken)
{
BTB[i].state++;
if (BTB[i].state > 3) BTB[i].state = 3;
}
else
{
BTB[i].state--;
if (BTB[i].state < 0) BTB[i].state = 0;
}
int Ntarget;
bool Nstate = CheckBTB(i, Ntarget);
return (Ptarget != Ntarget);
}

bool CheckBTB(int i, int &target)		//return true: Branch is predicted to be Taken |||| False: Branch is predicted to not be Taken
{
bool taken = (BTB[i].state == 2 || BTB[i].state == 3);
target = taken ? BTB[i].targetPC : i + 1;
return taken;
}*/


bool processStream(istream& in) {
    string input;
    while (getline(in, input)) {
        rt = rd = rs = -1;
        if (!processLine(input)) {
            cout << "Syntax Error @ \"" << input << "\" \n";
            return false;
        }
    }
    for (int i = 0; i < encounteredLabels.size(); i++)
        if (!labelTable.count(encounteredLabels[i])) {
            cout << "Undefined Label: " << encounteredLabels[i] << '\n';
            return false;
        }
    /*	for (int i = 0; i < imem.size(); i++)1
    {
    imem[i].label1 = labelTable[imem[i].label1];
    }*/
    return true;
}

bool processLine(const string& line) {
    instruction temp;
    if (isR(line) || isJ(line) || isBLE(line) || isI(line) || isJR(line) || isJal(line) || isRe(line)) { //isLW/SW/ PROCESS
        program += to_string(imem.size());
        program += "   ";
        program += line;
        program += '\n';

        temp.set();
        imem.push_back(temp);
        return true;
    }
    else if (isLabel(line)) {
        labelTable[label] = imem.size();
        return true;
    }
    else if (false)
        return true; //spacecheck
    else
        return false;
}
