<?php

header("Access-Control-Allow-Origin: *");

class Question {
    public $id;
    public $question;
    public $A;
    public $B;
    public $C;
    public $D;
    public $reponse;
}


function create_question($q, $a, $b, $c, $d, $r)
{
    static $i = 1;
    $qr = new Question();
    $qr->question = $q;
    $qr->id = $i++;
    $qr->A = $a;
    $qr->B = $b;
    $qr->C = $c;
    $qr->D = $d;
    $qr->reponse = $r;
    return $qr;
};

$q[1] = create_question("En C99, que vaut -7 / 3 ?", "-3", "-2", "2", "Cela dépend de l'implémentation", 2);
$q[2] = create_question("En C99, que vaut -7 % 3 ?", "1", "-1", "2", "Comportement indéfini", 2);
$q[3] = create_question("Que vaut (unsigned int)-1 ?", "0", "INT_MAX", "UINT_MAX", "Comportement indéfini", 3);
$q[4] = create_question("Que vaut 0u - 1u ?", "-1 de type int", "UINT_MAX", "0", "Comportement indéfini", 2);
$q[5] = create_question("Que provoque INT_MAX + 1 lorsque l'addition est effectuée en int ?", "INT_MIN", "UINT_MAX", "Comportement indéfini", "Une erreur de compilation obligatoire", 3);
$q[6] = create_question("En C99, le résultat de -8 >> 1 est :", "Toujours -4", "Toujours 4", "Comportement indéfini", "Défini par l'implémentation", 4);
$q[7] = create_question("Que provoque 1u << (sizeof(unsigned) * CHAR_BIT) ?", "0", "UINT_MAX", "Comportement indéfini", "Toujours 1", 3);
$q[8] = create_question("Que vaut ~0u ?", "UINT_MAX", "0", "INT_MIN", "Cela dépend de sizeof(unsigned)", 1);
$q[9] = create_question("Quelle valeur vaut toujours sizeof(char) ?", "1", "8", "CHAR_BIT", "sizeof(unsigned char)", 1);
$q[10] = create_question("Que vaut sizeof(\"42\") ?", "2", "3", "4", "sizeof(char *)", 2);
$q[11] = create_question("Que vaut strlen(\"A\\0B\") ?", "1", "2", "3", "4", 1);
$q[12] = create_question("Que vaut sizeof(\"A\\0B\") ?", "1", "2", "3", "4", 4);
$q[13] = create_question("Que produit char s[3] = \"abc\"; en C99 ?", "Une contrainte violée car il manque la place pour \\0", "Un tableau valide de 3 char sans terminateur nul", "Un tableau de 4 char", "Un comportement indéfini dès la déclaration", 2);
$q[14] = create_question("Avec char s[] = {'a', 'b', 'c'};, quelle affirmation est vraie ?", "sizeof(s) vaut 4", "s[3] vaut '\\0'", "sizeof(s) vaut 3 et aucun terminateur nul n'est ajouté", "La déclaration est invalide", 3);
$q[15] = create_question("Que provoque char *p = \"abc\"; p[0] = 'X'; ?", "La chaîne devient Xbc", "Une copie de la chaîne est créée", "Une erreur de compilation est obligatoire", "Comportement indéfini", 4);
$q[16] = create_question("Si int a[5];, quel est le type de &a ?", "int *", "int **", "int (*)[5]", "int *[5]", 3);
$q[17] = create_question("Si int a[5];, que représente a + 5 ?", "Un pointeur invalide dont la création est déjà UB", "Le pointeur one-past, valide à former mais pas à déréférencer", "L'adresse de a[5], qui est un int valide", "La même adresse que a + 4", 2);
$q[18] = create_question("Si int a[5];, l'expression &a[5] est :", "Valide et produit le pointeur one-past", "UB car a[5] est forcément lu", "Une contrainte violée", "Équivalente à &a[4]", 1);
$q[19] = create_question("Si int a[5];, que provoque int x = a[5]; ?", "x vaut 0", "x reçoit une valeur non spécifiée mais sans UB", "Une erreur de compilation obligatoire", "Comportement indéfini", 4);
$q[20] = create_question("Que fait int *p = (int *)0; int *q = &*p; ?", "UB car le pointeur nul est nécessairement déréférencé", "C'est défini et q reçoit la même valeur que p", "q pointe vers un int temporaire", "C'est une contrainte violée", 2);
$q[21] = create_question("Si int a[5];, de combien d'éléments de type int avance conceptuellement &a + 1 ?", "1", "4", "5", "sizeof(int *)", 3);
$q[22] = create_question("Quel est le type du résultat de p - q si p et q sont des pointeurs comparables dans le même tableau ?", "int", "size_t", "long", "ptrdiff_t", 4);
$q[23] = create_question("En C99, que signifie une déclaration int f(); ?", "f ne prend aucun argument", "Les paramètres de f ne sont pas spécifiés par cette déclaration", "f prend un argument int implicite", "La syntaxe est invalide en C99", 2);
$q[24] = create_question("Que signifie int f(void); en C99 ?", "f ne prend aucun paramètre", "f prend un void", "f prend un nombre quelconque d'arguments", "f retourne void puis est convertie en int", 1);
$q[25] = create_question("Dans void f(int a[10]);, en quel type le paramètre a est-il ajusté ?", "int [10]", "int (*)[10]", "int *", "int **", 3);
$q[26] = create_question("Que signifie le 10 dans void f(int a[static 10]) en C99 ?", "a est un tableau statique global", "La fonction alloue automatiquement 10 int", "a contient exactement 10 int", "À chaque appel, l'argument doit donner accès au premier élément d'au moins 10 éléments", 4);
$q[27] = create_question("Dans void f(int a[const 10]), quel est le type ajusté du paramètre a dans la fonction ?", "const int *", "int * const", "const int * const", "int (*)[10]", 2);
$q[28] = create_question("Avec int n = 3;, que fait sizeof(int[n++]) en C99 ?", "n reste 3 car sizeof n'évalue jamais son opérande", "Le programme est invalide", "n devient 4 et la taille calculée correspond à 3 int", "n devient 4 et la taille calculée correspond à 4 int", 3);
$q[29] = create_question("À portée de bloc, quelle est la durée de vie de l'objet créé par (int){42} ?", "Jusqu'à la fin de l'expression complète", "Jusqu'à la fin du bloc englobant", "Statique jusqu'à la fin du programme", "L'objet n'a aucune adresse", 2);
$q[30] = create_question("En C99, l'expression (int[]){1, 2, 3} est :", "Une lvalue représentant un objet tableau", "Une constante non adressable", "Un pointeur de type int *", "Une extension GNU", 1);
$q[31] = create_question("Après int a[5] = {[2] = 7};, quelles valeurs sont garanties ?", "Tous les éléments sauf a[2] sont indéterminés", "a[0] vaut 2 et a[2] vaut 7", "Seul a[2] est initialisé, lire les autres est UB", "a[0], a[1], a[3] et a[4] valent 0, a[2] vaut 7", 4);
$q[32] = create_question("Avec struct S { int a; int b; }; struct S s = {.b = 3};, que vaut s.a ?", "Valeur indéterminée", "0", "3", "Comportement indéfini à la lecture", 2);
$q[33] = create_question("Quelle déclaration utilise correctement un flexible array member C99 ?", "struct S { int a[]; int n; };", "struct S { int a[]; };", "struct S { int n; int a[]; };", "struct S { int a[0]; int n; };", 3);
$q[34] = create_question("Que peut-on dire de sizeof(struct S) pour struct S { size_t n; int data[]; }; ?", "Il n'inclut aucun élément réel de data[]", "Il vaut toujours sizeof(size_t)", "Il inclut automatiquement un int", "La structure n'a pas de taille en C99", 1);
$q[35] = create_question("En C99 strict, que vaut la déclaration int a[0]; pour un tableau ordinaire de taille constante ?", "Elle crée un pointeur", "Elle crée légalement un tableau vide", "Elle crée un VLA", "Elle viole une contrainte du standard", 4);
$q[36] = create_question("En C99 strict, struct S {}; est :", "Une structure valide de taille 0", "Non conforme : une structure doit avoir des membres", "Équivalente à struct S { char _; };", "Valide seulement à portée globale", 2);
$q[37] = create_question("Que provoque memcpy(buf + 1, buf, 5) si les deux zones de 5 octets se chevauchent ?", "Un déplacement garanti de gauche à droite", "Un déplacement garanti de droite à gauche", "Comportement indéfini", "Exactement le même comportement que memmove", 3);
$q[38] = create_question("Quelle fonction C99 est prévue pour copier des zones mémoire qui peuvent se chevaucher ?", "memmove", "memcpy", "strcpy", "bcopy", 1);
$q[39] = create_question("Deux structures ont exactement les mêmes valeurs de membres. memcmp(&a, &b, sizeof a) == 0 est-il garanti ?", "Oui, toujours", "Oui seulement si elles ont le même type", "Oui si aucun membre n'est un pointeur", "Non, notamment à cause des octets de padding", 4);
$q[40] = create_question("Que garantit memset(&p, 0, sizeof p) pour un objet pointeur p ?", "p devient forcément NULL", "Une représentation tout-bits-zéro est écrite, mais elle n'est pas garantie d'être le pointeur nul", "p pointe vers l'adresse 0 de façon portable", "memset ne peut pas être utilisé sur un pointeur", 2);
$q[41] = create_question("Que garantit calloc concernant la zone allouée ?", "Tous les objets pointeurs contenus deviennent forcément NULL", "Tous les double deviennent forcément 0.0 sur toute implémentation C99", "Tous les bits de la zone allouée sont initialisés à zéro", "La mémoire est remplie avec le caractère ASCII '0'", 3);
$q[42] = create_question("En C99, que peut faire malloc(0) ?", "Il doit retourner NULL", "Il doit retourner une adresse d'un octet", "Il provoque un comportement indéfini", "Le comportement est défini par l'implémentation : notamment NULL ou un pointeur non nul qui ne doit pas être utilisé pour accéder à un objet", 4);
$q[43] = create_question("Avec int i = 1; int r = i++ && i++;, quelles valeurs obtient-on ?", "i = 2, r = 0", "i = 2, r = 1", "i = 3, r = 1", "Comportement indéfini", 3);
$q[44] = create_question("Avec int i = 0; int r = i++ || i++;, quelles valeurs obtient-on ?", "i = 1, r = 0", "i = 2, r = 1", "i = 1, r = 1", "Comportement indéfini", 2);
$q[45] = create_question("Avec int i = 0; int r = (i++, i++);, quelles valeurs obtient-on ?", "i = 1, r = 0", "i = 2, r = 2", "Comportement indéfini", "i = 2, r = 1", 4);
$q[46] = create_question("Que provoque int i = 1; i = i++; en C99 ?", "Comportement indéfini", "i vaut forcément 1", "i vaut forcément 2", "Le résultat est non spécifié mais le comportement reste défini", 1);
$q[47] = create_question("Avec int i = 7; size_t n = sizeof(i++);, que se passe-t-il ?", "i devient 8 et n vaut sizeof(int)", "i reste 7 et n vaut sizeof(int)", "i devient 8 et n vaut sizeof(int *)", "Comportement indéfini", 2);
$q[48] = create_question("Avec #define SQR(x) ((x) * (x)), que provoque SQR(i++) ?", "i est incrémenté une seule fois", "i est incrémenté exactement deux fois de façon définie", "Comportement indéfini à cause des deux modifications non séquencées", "Le préprocesseur refuse l'expansion", 3);
$q[49] = create_question("Comment est analysée l'expression x & 1 == 0 ?", "(x & 1) == 0", "(x & 1 == 0) avec priorité gauche-droite uniforme", "(x & 1) == (0)", "x & (1 == 0), donc l'expression vaut 0", 4);
$q[50] = create_question("En C99 strict, avec int x; int *p = &x;, que penser de printf(\"%p\", p); ?", "C'est strictement correct car tout pointeur est automatiquement converti en void * dans ...", "Le format %p attend un void * ; sans cast vers void *, le type de l'argument variadique ne correspond pas et le comportement est indéfini", "C'est correct uniquement si sizeof(int *) == sizeof(void *)", "printf attend un int * pour %p", 2);
$q[51] = create_question("Avec int i = -1; unsigned int u = 1; que vaut i < u ?", "Vrai car -1 < 1", "Faux car i est converti en unsigned int", "Comportement indéfini", "Cela dépend de sizeof(int)", 2);
$q[52] = create_question("Que provoque register int a[3]; int *p = a; en C99 ?", "p pointe normalement vers a[0]", "Erreur obligatoire car register interdit les tableaux", "Comportement indéfini lors de la conversion du tableau register en pointeur", "a est copié dans un tableau temporaire", 3);
$q[53] = create_question("À portée fichier, si l'unique déclaration est int a[];, que devient a à la fin de l'unité de traduction ?", "Une simple déclaration sans définition", "Un tableau incomplet inutilisable", "Un tableau d'un seul int initialisé à zéro", "Une erreur obligatoire", 3);
$q[54] = create_question("À portée fichier, que produit static int a[]; en C99 strict ?", "Un tableau statique d'un élément", "Une déclaration externe", "Un VLA statique", "Une violation de contrainte car une définition tentative à linkage interne ne peut pas rester de type incomplet", 4);
$q[55] = create_question("À portée fichier, quel linkage possède const int x = 42; en C99 ?", "Aucun linkage", "Linkage interne comme en C++", "Linkage externe", "Cela dépend de const", 3);
$q[56] = create_question("Que fournit inline int f(void) { return 1; } à portée fichier en C99, sans static ni extern ?", "Une fonction à linkage interne", "Une définition externe complète suffisante à elle seule dans tous les cas", "Une définition inline d'une fonction à linkage externe, mais pas une définition externe", "Une macro équivalente à f", 3);
$q[57] = create_question("Que provoque f(&x, &x) pour void f(int *restrict p, int *restrict q) { *p = 1; *q = 2; } ?", "x vaut obligatoirement 2", "x vaut obligatoirement 1", "Comportement indéfini à cause de la violation des exigences de restrict", "Erreur de compilation obligatoire", 3);
$q[58] = create_question("Une zone suffisamment grande et alignée vient de malloc. Après *(int *)p = 42;, quel effective type cette écriture donne-t-elle à l'objet pour les accès ultérieurs ?", "void", "unsigned char", "int", "Aucun, malloc interdit la notion d'effective type", 3);
$q[59] = create_question("Une zone sans type déclaré obtenue par malloc reçoit via memcpy la représentation complète d'un objet int. Pour les accès ultérieurs ne modifiant pas cette zone, quel effective type est utilisé ?", "char", "void", "Le type de l'objet source, donc int", "Aucun type n'est établi par memcpy", 3);
$q[60] = create_question("Que produit p < q si p et q pointent vers deux objets indépendants sans relation de tableau entre eux ?", "0 obligatoirement", "Une comparaison définie de leurs adresses numériques", "Un résultat non spécifié mais comportement défini", "Comportement indéfini", 4);
$q[61] = create_question("Que peut valoir l'expression \"abc\" == \"abc\" en C99 ?", "Toujours 1", "Toujours 0", "0 ou 1 car il n'est pas spécifié si les deux littéraux désignent des tableaux distincts", "Comportement indéfini", 3);
$q[62] = create_question("Avec struct S { unsigned int x : 3; }; struct S s;, que produit sizeof(s.x) en C99 ?", "sizeof(unsigned int)", "1", "Une violation de contrainte car sizeof ne peut pas être appliqué à un membre bit-field", "Une valeur définie par l'implémentation", 3);
$q[63] = create_question("Quel est le type d'un identificateur de constante d'énumération comme RED dans enum Color { RED, GREEN }; en C99 ?", "enum Color", "int", "unsigned int", "Le plus petit type pouvant contenir sa valeur", 2);
$q[64] = create_question("Une union contient struct A { int x; double a; } et struct B { int x; char b; }. Après avoir stocké dans le membre A, peut-on lire le x du membre B ?", "Jamais, lire un membre inactif est toujours UB", "Oui, car x appartient à la common initial sequence des deux structures", "Seulement si sizeof(double) == sizeof(char)", "Seulement avec une extension Clang", 2);
$q[65] = create_question("Que se passe-t-il si un goto saute depuis l'extérieur vers l'intérieur de la portée d'un VLA déclaré dans ce bloc ?", "Le VLA est automatiquement créé lors du goto", "Le VLA possède une taille indéterminée", "Comportement indéfini uniquement à l'exécution", "Violation de contrainte : ce saut n'est pas permis", 4);
$q[66] = create_question("Que provoque int *f(void) { return &(int){42}; } puis int x = *f(); ?", "x vaut 42 de manière garantie", "Le compound literal possède une durée statique", "Comportement indéfini car l'objet du compound literal avait une durée automatique terminée au retour de f", "Erreur obligatoire à la compilation de f", 3);
$q[67] = create_question("Après longjmp, que devient une variable automatique locale non volatile de la fonction ayant appelé setjmp, si elle a été modifiée depuis setjmp ?", "Elle reprend obligatoirement sa valeur au moment de setjmp", "Elle conserve obligatoirement sa dernière valeur", "Sa valeur devient indéterminée", "Elle vaut zéro", 3);
$q[68] = create_question("Avec struct S { int n; int data[]; };, que produit struct T { int x; struct S s; }; en C99 strict ?", "Une structure valide avec un flexible array member imbriqué", "Une violation de contrainte car une structure contenant un flexible array member ne peut pas être membre d'une autre structure", "s est automatiquement converti en pointeur", "Valide uniquement si s est le dernier membre de T", 2);
$q[69] = create_question("Avec #define A 42 et #define STR(x) #x, que produit STR(A) ?", "La chaîne 42", "La chaîne A", "Le token A sans guillemets", "Erreur car # ne peut pas être utilisé sur un paramètre de macro", 2);
$q[70] = create_question("Avec #define X 12 et #define CAT(a,b) a##b, en supposant X3 non défini, que produit CAT(X,3) ?", "Le token 123", "Le token X3", "Les deux tokens X et 3", "Erreur de préprocesseur", 2);

if (!isset($_GET['question']) && !isset($_GET['check']))
    die("ok");

if ($_GET['question'])
{
    header("Content-type: application/json");
    $idx = random_int(1, 70);
    sleep(1);
    echo json_encode($q[$idx]);
    exit();
}

if ($_GET['check'] && $_GET['res'])
{
    $idx = htmlspecialchars($_GET['check']);
    $res = htmlspecialchars($_GET['res']);
    if (!ctype_digit($idx) || !ctype_digit($res))
        die("ko");
    foreach ($q as $elm)
    {
        if ($idx == $elm->id)
        {
            header("Content-type: application/json");
            if ($res == $elm->reponse)
                echo json_encode(["res" => "true"]);
            else
                echo json_encode(["res" => "false"]);
            exit();
        }
    }
    die("ko");
}
