#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <regex>
#include <algorithm>

class BinaryCompiler {
private:
    std::vector<uint8_t> binary;
    std::map<std::string, uint8_t> registers;
    
    // x86-64 Register encoding
    void initializeRegisters() {
        registers = {
            {"rax", 0x0}, {"rcx", 0x1}, {"rdx", 0x2}, {"rbx", 0x3},
            {"rsp", 0x4}, {"rbp", 0x5}, {"rsi", 0x6}, {"rdi", 0x7}
        };
    }
    
    uint8_t getRegisterCode(const std::string& reg) {
        std::string lower_reg = reg;
        std::transform(lower_reg.begin(), lower_reg.end(), lower_reg.begin(), ::tolower);
        
        auto it = registers.find(lower_reg);
        if (it == registers.end()) {
            throw std::runtime_error("Unknown register: " + reg);
        }
        return it->second;
    }
    
    bool isImmediate(const std::string& operand) {
        return std::isdigit(operand[0]) || (operand.length() > 2 && operand.substr(0, 2) == "0x");
    }
    
    uint64_t parseImmediate(const std::string& operand) {
        if (operand.substr(0, 2) == "0x") {
            return std::stoull(operand, nullptr, 16);
        }
        return std::stoull(operand, nullptr, 10);
    }
    
    // MOV reg, imm64
    void encodeMov_reg_imm(const std::string& reg, uint64_t imm) {
        binary.push_back(0x48);                          // REX.W prefix
        binary.push_back(0xB8 + getRegisterCode(reg));  // MOV opcode + reg
        
        // 64-bit immediate (little-endian)
        for (int i = 0; i < 8; i++) {
            binary.push_back((imm >> (i * 8)) & 0xFF);
        }
    }
    
    // ADD reg, imm32
    void encodeAdd_reg_imm(const std::string& reg, uint32_t imm) {
        binary.push_back(0x48);                          // REX.W prefix
        binary.push_back(0x81);                          // ADD r64, imm32 opcode
        
        uint8_t modrm = 0xC0 | getRegisterCode(reg);
        binary.push_back(modrm);
        
        // 32-bit immediate (little-endian)
        for (int i = 0; i < 4; i++) {
            binary.push_back((imm >> (i * 8)) & 0xFF);
        }
    }
    
    // ADD reg1, reg2
    void encodeAdd_reg_reg(const std::string& dst, const std::string& src) {
        binary.push_back(0x48);                          // REX.W prefix
        binary.push_back(0x01);                          // ADD r64, r64 opcode
        
        uint8_t modrm = 0xC0 | (getRegisterCode(src) << 3) | getRegisterCode(dst);
        binary.push_back(modrm);
    }
    
    // SUB reg, imm32
    void encodeSub_reg_imm(const std::string& reg, uint32_t imm) {
        binary.push_back(0x48);                          // REX.W prefix
        binary.push_back(0x81);                          // SUB r64, imm32 opcode
        
        uint8_t modrm = 0xC0 | 0x28 | getRegisterCode(reg);
        binary.push_back(modrm);
        
        // 32-bit immediate (little-endian)
        for (int i = 0; i < 4; i++) {
            binary.push_back((imm >> (i * 8)) & 0xFF);
        }
    }
    
    // RET
    void encodeRet() {
        binary.push_back(0xC3);
    }
    
    // PUSH reg
    void encodePush(const std::string& reg) {
        binary.push_back(0x50 + getRegisterCode(reg));
    }
    
    // POP reg
    void encodePop(const std::string& reg) {
        binary.push_back(0x58 + getRegisterCode(reg));
    }
    
    // NOP
    void encodeNop() {
        binary.push_back(0x90);
    }
    
public:
    BinaryCompiler() {
        initializeRegisters();
    }
    
    void compileInstruction(const std::string& instruction) {
        std::string lower_instr = instruction;
        std::transform(lower_instr.begin(), lower_instr.end(), lower_instr.begin(), ::tolower);
        
        // Parse instruction
        std::istringstream iss(lower_instr);
        std::string opcode, operand1, operand2;
        iss >> opcode >> operand1 >> operand2;
        
        // Remove commas from operands
        operand1.erase(std::remove(operand1.begin(), operand1.end(), ','), operand1.end());
        operand2.erase(std::remove(operand2.begin(), operand2.end(), ','), operand2.end());
        
        if (opcode == "mov") {
            if (operand2.empty()) throw std::runtime_error("MOV requires 2 operands");
            if (isImmediate(operand2)) {
                encodeMov_reg_imm(operand1, parseImmediate(operand2));
            } else {
                throw std::runtime_error("Only MOV reg, imm supported");
            }
        }
        else if (opcode == "add") {
            if (operand2.empty()) throw std::runtime_error("ADD requires 2 operands");
            if (isImmediate(operand2)) {
                encodeAdd_reg_imm(operand1, (uint32_t)parseImmediate(operand2));
            } else {
                encodeAdd_reg_reg(operand1, operand2);
            }
        }
        else if (opcode == "sub") {
            if (operand2.empty()) throw std::runtime_error("SUB requires 2 operands");
            if (isImmediate(operand2)) {
                encodeSub_reg_imm(operand1, (uint32_t)parseImmediate(operand2));
            } else {
                throw std::runtime_error("Only SUB reg, imm supported");
            }
        }
        else if (opcode == "ret") {
            encodeRet();
        }
        else if (opcode == "push") {
            encodePush(operand1);
        }
        else if (opcode == "pop") {
            encodePop(operand1);
        }
        else if (opcode == "nop") {
            encodeNop();
        }
        else if (opcode.empty() || opcode[0] == ';') {
            // Skip comments and empty lines
        }
        else {
            throw std::runtime_error("Unknown instruction: " + opcode);
        }
    }
    
    void compile(const std::string& assembly) {
        binary.clear();
        std::istringstream iss(assembly);
        std::string line;
        
        while (std::getline(iss, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == ';') {
                continue;
            }
            
            // Remove inline comments
            size_t comment_pos = line.find(';');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
                line.erase(line.find_last_not_of(" \t") + 1);
            }
            
            if (!line.empty()) {
                compileInstruction(line);
            }
        }
    }
    
    std::string toHexString() const {
        std::ostringstream oss;
        for (size_t i = 0; i < binary.size(); i++) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)binary[i];
            if (i < binary.size() - 1) oss << " ";
        }
        return oss.str();
    }
    
    std::string toBinaryString() const {
        std::ostringstream oss;
        for (uint8_t byte : binary) {
            for (int i = 7; i >= 0; i--) {
                oss << ((byte >> i) & 1);
            }
        }
        return oss.str();
    }
    
    void saveBinary(const std::string& filename) const {
        std::ofstream file(filename, std::ios::binary);
        file.write(reinterpret_cast<const char*>(binary.data()), binary.size());
        file.close();
        std::cout << "Binary saved to " << filename << std::endl;
    }
    
    size_t getSize() const {
        return binary.size();
    }
    
    const std::vector<uint8_t>& getBinary() const {
        return binary;
    }
};

int main() {
    BinaryCompiler compiler;
    
    std::string assembly = R"(
        ; Fast x86-64 Assembly Compilation
        MOV RAX, 10
        MOV RBX, 5
        ADD RAX, RBX
        ADD RAX, 20
        SUB RAX, 3
        PUSH RAX
        POP RDX
        RET
    )";
    
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║   Binary Machine Code Compiler (C++)   ║\n";
    std::cout << "║   FAST BOY MAN! ⚡                     ║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";
    
    try {
        std::cout << "📝 Input Assembly:\n";
        std::cout << assembly << "\n\n";
        
        compiler.compile(assembly);
        
        std::cout << "🔢 Machine Code (HEX):\n";
        std::cout << compiler.toHexString() << "\n\n";
        
        std::cout << "📊 Machine Code (BINARY):\n";
        std::cout << compiler.toBinaryString() << "\n\n";
        
        std::cout << "📈 Stats:\n";
        std::cout << "   Total bytes: " << compiler.getSize() << "\n";
        std::cout << "   Total bits: " << (compiler.getSize() * 8) << "\n\n";
        
        compiler.saveBinary("output.bin");
        
        std::cout << "✅ Compilation complete!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
