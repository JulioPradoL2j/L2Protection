<div align="center">

# 🛡️ L2Protection

### Advanced Client Protection for Lineage II Interlude

Sistema de proteção para servidores Lineage II desenvolvido para auxiliar na detecção e mitigação de modificações não autorizadas no cliente do jogo.

![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Language](https://img.shields.io/badge/C%2B%2B-Native-orange)
![Version](https://img.shields.io/badge/Status-Active-success)
![Release](https://img.shields.io/badge/Build-Stable-brightgreen)

</div>

---

# 📖 Sobre

O **L2Protection** é uma solução desenvolvida para aumentar a segurança do cliente Lineage II, auxiliando na proteção contra modificações não autorizadas e garantindo maior integridade entre cliente e servidor.

O projeto foi desenvolvido para integração simples com servidores Interlude e pode ser utilizado juntamente com launcher e sistemas de atualização.

---

# ✨ Recursos

- ✅ Proteção baseada em DLL
- ✅ Inicialização automática pelo cliente
- ✅ Compatível com Windows 10 e Windows 11
- ✅ Compatível com servidores Interlude
- ✅ Integração com Launcher
- ✅ Integração com Updater
- ✅ Sistema leve e de baixo consumo
- ✅ Atualização simplificada

---

# 📥 Download

## Repositório Oficial

https://github.com/JulioPradoL2j/L2Protection

## Arquivos Compilados

https://github.com/JulioPradoL2j/CompiledFiles

---

# 📦 Estrutura do Projeto

```text
L2Protection/
│
├── build/
├── dist/
├── dsetup/
├── update.dll
└── dsetup.sln
```

| Pasta | Descrição |
|---------|---------|
| build | Arquivos intermediários de compilação |
| dist | Binários gerados |
| dsetup | Projeto principal |
| update.dll | Biblioteca principal |
| dsetup.sln | Solução Visual Studio |

---

# ⚙️ Requisitos

## Sistema Operacional

- Windows 10
- Windows 11
- Windows Server 2019+
- Windows Server 2022

## Ferramentas de Desenvolvimento

- Visual Studio 2019 ou superior
- Windows SDK
- Runtime C++

---

# 🔨 Compilação

Abra a solução:

```text
dsetup.sln
```

No Visual Studio:

```text
Build
 └── Build Solution
```

Ou utilize:

```cmd
MSBuild dsetup.sln /p:Configuration=Release
```

Os binários serão gerados em:

```text
dist\
```

---

# 🚀 Instalação

Copie os arquivos compilados para a pasta do cliente Lineage II conforme a documentação do servidor.

Após a instalação:

1. Atualize os arquivos através do launcher.
2. Reinicie o cliente.
3. Verifique se a proteção foi carregada corretamente.

---

# 🔄 Atualização

Para atualizar:

1. Substitua os arquivos da versão anterior.
2. Execute o atualizador.
3. Reinicie o cliente.

---

# 📁 Projetos Relacionados

| Projeto | Link |
|----------|----------|
| Launcher | https://github.com/JulioPradoL2j/L2Updater |
| Atualizador | https://github.com/JulioPradoL2j/CompiledFiles |
| InterfaceBlock | https://github.com/JulioPradoL2j/InterfaceBlock |
| Website | https://github.com/JulioPradoL2j/L2UpdaterWeb |

---

# ⚠️ Aviso

Este projeto é disponibilizado exclusivamente para fins educacionais, estudo e desenvolvimento de servidores Lineage II.

O autor não se responsabiliza pelo uso inadequado do software.

---

# 📞 Contato

GitHub:

https://github.com/JulioPradoL2j

E-mail:

juliopradol2j@gmail.com

---

<div align="center">

### © L2Protection Project

Security Layer for Lineage II Servers

</div>