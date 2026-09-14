# K2M — Checkpoint de progresso

> Registro curto de estado, conforme pedido na seção 21 do
> `K2M-Arquitetura-e-Plano.md`. Atualizar a cada sessão relevante, não é
> changelog completo — só o suficiente para retomar o trabalho sem
> reconstruir contexto do zero. Consultar `git log` para histórico real.

**Última atualização:** 2026-09-14, sessão no PC de trabalho (sem Kontakt/MODX M instalados). Continuação prevista: notebook pessoal com Kontakt e Ableton instalados.

## Estado por fase (numeração da seção 20 do plano)

- **Fase 0/1 (Skeleton + Host mínimo):** concluídas. `K2M.exe` compila e abre; `PluginHost` faz scan/instancia VST3 assíncrono.
- **Fase 2 (Gate A):** **fechada com ressalva.** `GateATest.exe` rodou 10 takes consecutivos (10 processos novos) com `K2M_TestSynth` (plugin de teste controlado, não Kontakt) — WAVs bit-a-bit idênticos, sem crash/nota presa/truncamento/alteração de ganho. **Kontakt real nunca foi testado nesta máquina porque não está instalado aqui.** Isso é exatamente o cenário que a seção 21 do plano previu ("conclua tudo que puder ser validado com fonte de teste... mantenha Gate A como pendente"). **Ação em casa:** rodar `GateATest.exe` e, se possível, `GateBFixtureTest.exe` apontando para o Kontakt real (basta ele estar instalado em `C:\Program Files\Common Files\VST3` — o scan já prioriza plugins com "Kontakt" no nome). Testar também nos 3 presets da seção 4.7 (percussivo, sustentado, release audível), não só num instrumento de teste.
- **Fase 3 (Gate B pequeno):** em andamento.
  - Fixture de 4 WAV reais (A2/C3 × velocity 64/127) + SFZ portável gerados por `GateBFixtureTest.exe` (usa `K2M_TestSynth` por padrão; se achar Kontakt no scan, usa Kontakt automaticamente — ver `usingRealKontakt` em `Tests/GateBFixtureTest.cpp`).
  - **Bug real encontrado e corrigido:** `SfzExporter` escrevia o opcode inválido `ampeg_veltrack=0` (não existe no SFZ; o certo é `amp_veltrack=0`). Sem o fix, toda exportação SFZ reaplicava a curva de velocity padrão do player por cima da dinâmica já capturada. Corrigido em `Source/Export/SfzExporter.cpp` e no exemplo da seção 13 do plano.
  - **Validação sem MODX M físico (opção 1, concluída):** `Tests/SfzEngineValidationTest.cpp` renderiza o SFZ do fixture através de uma engine SFZ independente (sfizz, LGPLv3) e confirma pitch/keyrange corretos (erro <2Hz) e ganho consistente entre as 4 regiões (prova que o fix do `amp_veltrack` funciona e que não há curva de velocity/pitch indevida por região). **Não prova nada sobre import no MODX M** — só que o SFZ gerado é espec-compliant para um parser real.
  - **Pendente (opção 2, combinada com o usuário mas não iniciada):** baixar e rodar o ConvertWithMoss (Java, LGPLv3 — https://github.com/git-moss/ConvertWithMoss) para converter de fato `export/sfz/k2m_fixture.sfz` num arquivo legado Yamaha (Y2L/Y2U), e inspecionar o que ele aceita/perde antes de ter o MODX M em mãos.
  - **Pendente (Gate B em si):** import físico no MODX M — duas notas × duas velocities corretas, memória ocupada, firmware registrado. Precisa do teclado físico (não mencionado como disponível em casa; se continuar indisponível, deixar pendente e seguir com o resto da Fase 3/4 que não depende dele).

## Bug de infraestrutura corrigido nesta sessão

`.gitignore` tinha o padrão `export/` sem barra inicial, que no filesystem case-insensitive do Windows também batia com `Source/Export/`. Resultado: **`Source/Export/SfzExporter.cpp` e `.h` nunca foram commitados**, apesar do commit `7cacc55` alegar tê-los adicionado. Corrigido ancorando todos os padrões de diretório gerado (`/build/`, `/export/`, `/temp/`, `/logs/`, `/raw/`, `/processed/`, `/bin/`, `/out/`, `/.cache/`) e os arquivos foram resgatados/commitados. **Se notar algo "sumido" do repo depois de um `git status` limpo, é sinal do mesmo tipo de problema — sempre rodar `git ls-files` vs `find`/`ls` real para comparar.**

## Setup necessário para retomar em outra máquina

1. Clonar (se ainda não clonado) e/ou `git pull` no repo.
2. Submodule do JUCE: `git submodule update --init --recursive` (o build depende de `external/JUCE`, tag 8.0.12 fixada — não deixar mudar para `master`).
3. `cmake -S . -B build -A x64` e `cmake --build build --config Release`.
4. **`external/sfizz_render/` não é versionado** (binário de terceiros, .gitignore). Se quiser rerodar `SfzEngineValidationTest.exe`, baixar de novo:
   `https://github.com/sfztools/sfizz/releases/download/1.2.3/sfizz-1.2.3-win64.zip`, extrair `bin/Release/sfizz_render.exe` e `bin/Release/sfizz.dll` para `external/sfizz_render/`. Sem esses arquivos o teste avisa e sai com código 0 (não bloqueia o resto do build).
5. Kontakt precisa estar instalado e autorizado via Native Access; o scan do `PluginHost` já cobre `C:\Program Files\Common Files\VST3` por padrão.

## Próximos passos sugeridos (em ordem)

1. Com Kontakt disponível: rerodar `GateATest.exe` (10 takes) e os 3 presets da seção 4.7 contra o Kontakt real, não só o `K2M_TestSynth`. Isso fecha de vez a ressalva da Fase 2.
2. Rodar `GateBFixtureTest.exe` com Kontakt real para um fixture com áudio de verdade (hoje é só o synth de teste) — abrir os WAVs resultantes no Ableton (parte do critério de aceite da seção 4.7: "reaberto por leitor independente e ouvido no Ableton").
3. Seguir com a opção 2 combinada: baixar/testar ConvertWithMoss na rota SFZ → Yamaha legado.
4. Gate B em si (import no MODX M) continua bloqueado até ter o teclado físico em mãos.
