# Relazione — Syscall Throttling (LKM)

Progetto del corso Advanced Operating Systems (and System Security), prof. Francesco Quaglia, Università di Roma Tor Vergata, A.A. 2025-2026.
Autrice: Ilaria Cialfi.

## 1. Introduzione e specifica

Il progetto realizza un modulo del kernel Linux (Linux Kernel Module, LKM) che implementa un meccanismo di *syscall throttling*: la limitazione della frequenza con cui determinate system call possono essere invocate, anche quando il sistema potrebbe eseguirle più rapidamente. Il modulo espone un char device la cui interfaccia, basata su `ioctl()`, permette all'amministratore di registrare e deregistrare tre tipi di **soggetti**: nomi di programma, user ID effettivi (EUID) e numeri di system call x86-64. 

Una system call registrata, quando è invocata da un programma registrato **oppure** da un EUID registrato, non viene eseguita direttamente ma passa attraverso un monitor fornito dal modulo. Il monitor applica una soglia **MAX** di invocazioni per finestra di tempo di 1 secondo: oltre MAX i thread chiamanti vengono **bloccati temporaneamente**, impedendo l'esecuzione effettiva della syscall fino alla conclusione dello slot di 1 secondo. Le syscall gestite possono essere di qualsiasi natura, bloccanti o non bloccanti.

La specifica richiede inoltre che la configurazione (programmi, EUID e syscall registrati) sia leggibile in ogni istante ma modificabile solo da un thread con EUID 0 (root); che il monitor possa essere acceso e spento a runtime, con lo spegnimento che annulla ogni limitazione; e che il modulo fornisca, in funzione del valore di MAX, due statistiche: il picco di ritardo nell'esecuzione effettiva di una syscall con il programma e l'EUID responsabili, e il numero medio e massimo di thread che è stato necessario bloccare. Fa parte della consegna anche il codice utente per configurare il monitor e verificarne il funzionamento.

## 2. Architettura del modulo

Il modulo è organizzato in un char device con un unico nodo di controllo (`/dev/throttler`), con interfaccia esclusivamente `ioctl` , e in quattro sorgenti più una libreria del corso:

- `main.c` — ingresso e uscita del modulo. `init_module` riceve l'indirizzo della `sys_call_table` come parametro, lo valida, avvia la finestra delle statistiche e registra il device; `cleanup_module` esegue lo spegnimento sicuro e deregistra il device.
- `device.c` — il char device e il dispatcher degli `ioctl`. I comandi di scrittura richiedono EUID 0; il comando di lettura della configurazione è aperto a tutti.
- `registry.c` — lo stato della configurazione (syscall, EUID e programmi registrati, MAX, stato del monitor), il conteggio delle invocazioni nella finestra di 1 secondo per singolo soggetto, e le statistiche.
- `sctable.c` — l'aggancio alla `sys_call_table`: installa un wrapper generico al posto di una syscall registrata, ne ripristina il puntatore originale, e contiene il wrapper stesso (che applica il throttling) e la logica di rimozione sicura.
- `lib/scth.c` — libreria del corso, usata per le funzioni `unprotect`/`protect` necessarie a scrivere nelle entry della tabella.

Due header condivisi: 
- `throttler.h`, interno al modulo (nome del modulo, dichiarazioni delle funzioni condivise fra i sorgenti, costanti dei risultati sul soggetto).
- `throttle_ioctl.h`, condiviso tra modulo e programmi utente (codici dei comandi `ioctl` e `struct throttle_config`, usata per leggere configurazione e statistiche).

Esistono due percorsi di esecuzione distinti:
* Il **percorso di configurazione** parte dal programma utente `throttlectl`, che invoca `ioctl()` su `/dev/throttler`: `device.c` riceve il comando, verifica l'EUID sui comandi di scrittura e lo smista verso `registry.c`, che aggiorna lo stato. La lettura di configurazione e statistiche, invece, avviene con un unico comando (`GET_CONFIG`), gestito prima del controllo dei permessi e quindi accessibile a chiunque.
* Il **percorso di una syscall monitorata** parte invece dall'invocazione della syscall stessa. Poiché l'entry della `sys_call_table` è stata sostituita, il dispatch del kernel chiama il wrapper generico in `sctable.c` al posto della syscall originale. Il wrapper, se il monitor è acceso, chiede a `registry.c` di contare l'invocazione per il soggetto (programma e/o EUID): se il soggetto è sotto MAX la chiamata prosegue e il wrapper esegue la syscall originale; se è oltre MAX il thread viene bloccato per il residuo della finestra e, al risveglio, ri-conteggiato. Al termine viene eseguita la syscall originale (salvo interruzione da segnale) e il wrapper ne restituisce il risultato.

## 3. Tecnica di aggancio della syscall

L'aggancio delle syscall avviene sostituendo, nella `sys_call_table`, il puntatore alla syscall originale con il puntatore a un wrapper del modulo. La tabella risiede in memoria mappata in sola lettura: per scriverci si usano le funzioni `unprotect_memory`/`protect_memory` della libreria `scth`, che disabilitano e riabilitano temporaneamente la protezione di scrittura (il bit WP di CR0) attorno alla modifica. L'indirizzo della tabella non è esportato dal kernel: viene fornito al modulo come parametro, letto dal discoverer `the_usctm` del professor Quaglia, che lo scrive in `/sys/module/the_usctm/parameters/sys_call_table_address`.

Tutte le syscall agganciate condividono un **unico wrapper generico**: dal kernel 4.17 ogni entry della tabella ha la stessa firma — `long (*)(const struct pt_regs *)`, con gli argomenti dentro `pt_regs` — quindi un solo puntatore a funzione basta a servirle tutte. Il wrapper distingue quale syscall è stata invocata leggendo `regs->orig_ax`, che su x86-64 contiene il numero della system call, e la elabora di conseguenza. Essendo installato nella tabella, il wrapper viene eseguito lungo il normale cammino della syscall, nel contesto del thread chiamante.

Per ogni syscall registrata il modulo salva il puntatore originale in un array dedicato (`original_syscalls[]`, indicizzato da uno slot che tiene traccia dei numeri agganciati): alla deregistrazione, o allo scaricamento del modulo, il puntatore originale viene riscritto nella tabella, ripristinandone lo stato iniziale. Il discoverer è quindi fondamentale per riuscire ad agganciare a runtime system call *esistenti e arbitrarie*, scelte via `ioctl`. 

## 4. Monitoraggio e throttling

Il monitor interviene solo quando ricorrono due condizioni insieme: la syscall invocata è tra quelle registrate **e** chi la invoca è un soggetto registrato — un programma registrato **oppure** un EUID registrato. Il programma è identificato tramite `current->comm`: è il nome del task (al più 15 caratteri utili, per il limite `TASK_COMM_LEN` = 16), non il percorso assoluto dell'eseguibile, perciò `add-prog` registra il nome breve. L'EUID è letto da `current->cred->euid.val`. Se la syscall non è registrata, oppure né il programma né l'EUID corrispondono, il wrapper esegue la syscall originale senza applicare alcun controllo.

La soglia MAX è applicata **per singolo soggetto registrato**, non come limite globale: ogni programma e ogni EUID registrato ha il proprio contatore e la propria finestra. Contatori separati consentono di misurare il picco di ritardo con il programma e l'EUID responsabili. Inoltre, un limite unico condiviso mescolerebbe soggetti indipendenti, poco coerente con il meccanismo indicato nella traccia, pensato per sicurezza e scalabilità a livello di singolo soggetto. 
Una chiamata che fa match sia per programma sia per EUID viene conteggiata su entrambi i soggetti e bloccata se almeno uno dei due supera MAX.

Il conteggio avviene su una **finestra fissa di 1 secondo**: ogni soggetto mantiene l'istante di apertura della propria finestra e un contatore di invocazioni. A ogni chiamata, se è trascorso più di 1 secondo dall'apertura, la finestra viene riaperta (istante aggiornato, contatore azzerato); poi, se il contatore è sotto MAX, la chiamata è ammessa e il contatore incrementato, altrimenti è oltre soglia.

Il tempo è misurato in **jiffies**. Un *jiffy* è l'unità di tempo elementare del kernel: esiste una variabile globale `jiffies` che viene incrementata a ogni *tick*. La costante `HZ` indica quanti tick avvengono in un secondo (la frequenza del timer), quindi **1 secondo corrisponde a `HZ` jiffies**. Confrontare il valore corrente di `jiffies` con l'istante di apertura della finestra permette così di misurare quanto tempo è trascorso; il confronto usa la macro `time_after`, che gestisce correttamente l'eventuale *wraparound* del contatore. 

Il valore di default di MAX è **illimitato** (`ULONG_MAX`): finché non si esegue `set-max`, nessuna chiamata viene limitata. Impostare esplicitamente `set-max 0` blocca invece tutte le chiamate registrate: con MAX = 0 la condizione "contatore sotto MAX" non è mai soddisfatta, quindi ogni invocazione risulta oltre soglia.

Quando un soggetto supera MAX, il thread chiamante viene **bloccato** per il tempo che manca alla fine della finestra corrente: il thread dorme su una wait queue tramite `wait_event_interruptible_timeout`, con timeout pari ai jiffies residui della finestra (`left`). Al risveglio il soggetto viene ri-conteggiato: se nel frattempo la finestra si è riaperta ed è tornato sotto MAX il thread prosegue, altrimenti il ciclo di attesa si ripete. Il ciclo si interrompe anche se, durante l'attesa, il monitor viene spento: in tal caso il thread viene rilasciato senza ulteriori limitazioni.

Poiché l'attesa è *interrompibile*, un **segnale** ricevuto mentre il thread è bloccato fa terminare `wait_event_interruptible_timeout` con `-ERESTARTSYS` (Error, RESTART SYScall = _la system call è stata interrotta da un segnale, va riavviata_). In questo caso il wrapper restituisce `-ERESTARTSYS` e **non** esegue la syscall originale.

## 5. Statistiche

Il **picco di ritardo** è il massimo ritardo imposto dal throttling a una singola invocazione bloccata. È misurato con `ktime_get()` — un orologio monotono ad alta risoluzione, in nanosecondi — prendendo l'istante prima e dopo il ciclo di blocco: la differenza è quindi il solo tempo trascorso in attesa, e **non** include l'esecuzione della syscall reale. Rispetto alla finestra, dove i jiffies bastano, qui serve una risoluzione fine perché il ritardo va riportato in millisecondi. Il valore massimo osservato è conservato insieme al programma e all'EUID responsabili della chiamata che lo ha causato.

Per i **thread bloccati** il modulo mantiene un contatore dei thread attualmente bloccati (`blocked_threads`), aggiornato sotto il mutex del registro: incrementato quando un thread entra nel blocco, decrementato quando ne esce. Da questo si ricavano le due statistiche richieste: 
* il **picco** (`peak_blocked_threads`), cioè il massimo valore raggiunto dal contatore, 
* la **media**, calcolata come somma di tutti i tempi di blocco (`total_blocked_ns`) divisa per la finestra di osservazione (`observation_win_ns`): è quindi la **somma** dei tempi di blocco di **tutti i thread** rapportata al tempo di osservazione, ovvero il **numero medio di thread bloccati** nel periodo. 

La **finestra di osservazione** (`observation_win_ns`) parte al caricamento del modulo e viene fatta ripartire a ogni accensione del monitor (`on`), così le statistiche descrivono la sessione di monitoring corrente e non l'intero uptime del modulo. Tutte le statistiche si leggono con lo stesso comando `GET_CONFIG`, gestito prima del controllo dell'EUID e quindi accessibile a qualunque utente.

## 6. Rimozione sicura

Poiché il modulo sostituisce le entry della `sys_call_table`, ripristinare la tabella e liberare le risorse del modulo mentre un thread sta ancora eseguendo il wrapper ne deallocherebbe il codice mentre è ancora in esecuzione, con conseguente crash del kernel. Per evitarlo il wrapper mantiene un contatore atomico delle chiamate in corso, `active_calls`, incrementato all’ingresso e decrementato su ogni via d’uscita e un flag `unloading` che segnala l'inizio dello scaricamento.

Lo spegnimento sicuro (`sctable_shutdown`, chiamato da `cleanup_module`) procede in ordine: alza `unloading`; sveglia i thread eventualmente bloccati (`wake_up_interruptible`) perché ricontrollino il flag ed escano dall'attesa; ripristina la tabella, così **nessuna nuova chiamata può entrare nel wrapper**; infine **drena** le chiamate ancora in corso, attendendo con `while (atomic_read(&active_calls) > 0) schedule();` finché il contatore non scende a zero. `schedule()` cede volontariamente la CPU e poi ricontrolla, lasciando avanzare i thread ancora dentro il wrapper. Solo a drenaggio concluso il device viene deregistrato.

Le **system call che non ritornano** — `exit`, `exit_group`, `rt_sigreturn` — sono rifiutate già in fase di registrazione. Non seguendo il normale cammino di ritorno, non eseguirebbero mai il decremento di `active_calls`: se agganciate, lascerebbero il contatore perennemente sopra lo zero e il drenaggio si bloccherebbe per sempre. Escluderle alla registrazione garantisce che ogni chiamata agganciata decrementi il contatore al ritorno, e quindi che il drenaggio termini.

**Limite correlato**: il drenaggio aspetta che _tutte_ le chiamate in corso ritornino — incluse quelle ferme dentro la syscall **originale** se è bloccante (es. un demone fermo in `write`). In quel caso lo scaricamento attende il ritorno di quella syscall: è sicuro (nessun crash), ma non istantaneo.

## 7. Test e misure

Il modulo è stato verificato con una matrice di test manuale ([matrice-test.pdf](docs/matrice-test.pdf)) organizzata in categorie: ciclo di vita del modulo (caricamenti/scaricamenti ripetuti), registrazione e percorsi d'errore, permessi (scrittura riservata a root, lettura aperta a tutti), filtro dei soggetti (chi viene osservato), throttling, riconfigurazione a caldo (spegnimento del monitor, deregistrazione o cambio di MAX con thread già bloccati), statistiche, rimozione sicura sotto carico e concorrenza. Ogni categoria ha prodotto le proprie evidenze (`dmesg`, output di `show`/`stats`, screenshot), raccolte nella documentazione del progetto. I test sono eseguiti dai programmi utente `test_ioctl`, `test_syscall_by_nr`, `test_args`, `test_blockingsys` e `test_blocked`, che coprono i diversi casi: syscall senza argomenti, con argomenti, bloccante, e carico concorrente di più processi.

Oltre ai test funzionali, due misure quantitative sono raccolte dallo script `test/run_tests.sh`, che fa variare un parametro per volta e scrive i risultati in CSV, da cui uno script Python genera i grafici.

**Effetto del throttling.** Il primo grafico riporta il tempo di completamento di 50 invocazioni di `getpid` al variare di MAX. Il tempo cala rapidamente al crescere di MAX: con MAX basso il throttling impone attese lunghe (le chiamate oltre soglia aspettano la fine della finestra), mentre con MAX ≥ 50 le 50 chiamate rientrano in un'unica finestra e si completano quasi istantaneamente. Il grafico conferma che il meccanismo limita il ritmo di invocazione in funzione di MAX.

![Effetto del throttling](test/output/throttling_effect.png)

**Contesa.** Il secondo grafico riporta il picco e la media dei thread bloccati al variare del numero di processi concorrenti, a MAX fisso (5). Tutti i processi sono istanze dello stesso programma e condividono quindi un unico budget MAX: quando la domanda supera di molto il budget, praticamente tutti i processi finiscono bloccati nello stesso istante, e il **picco satura al numero di processi**. La **media**, che cresce in modo sotto-lineare, è l'indicatore significativo della contesa: è il numero medio di thread bloccati contemporaneamente nel periodo. La saturazione del picco non è un difetto, ma la conseguenza attesa del budget condiviso.

![Thread bloccati in funzione della concorrenza](test/output/concurrency.png)
