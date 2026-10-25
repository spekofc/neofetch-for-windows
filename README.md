neofetch for windows

Um neofetch simples para Windows, escrito em C++ num único arquivo (neofetch.cpp). Usa só a WinAPI, sem dependências externas.

<img width="807" height="354" alt="{7577501A-1DBD-4BD2-BB3E-DD51245EEB6D}" src="https://github.com/user-attachments/assets/e84c3b94-ad19-4649-8ce7-37e0a7b5899f" />

O que mostra

Usuário@host, SO, modelo do computador, kernel/build, uptime, shell, resolução, terminal, CPU, GPU, memória, disco e locale.

Compilar

MinGW:

g++ -O2 -static neofetch.cpp -o neofetch.exe -luser32 -ladvapi32

MSVC:

cl /EHsc /O2 /utf-8 neofetch.cpp
Uso
neofetch.exe

Funciona no Windows 10 ou mais novo. Para as cores e o logo aparecerem direito, use o Windows Terminal.

Licença

MIT
