<?php
if (session_status() == PHP_SESSION_NONE)
    session_start();
if (isset($_GET['reset']) && $_GET['reset'] == 1)
{
    session_destroy();
    session_start();
}
if (isset($_POST['name']) && isset($_POST['good']))
{
    $good = $_POST['good'];
    $idx = 0;
    if ($good < 0 || $good > 5)
        die("gros caca");
    foreach ($_SESSION['user'] as $elm)
    {
        echo $elm;
        $idx++;
    }
    $_SESSION['user'][$idx]['name'] = $_POST['name'];
    $_SESSION['user'][$idx]['good'] = $good;
    header("location: http://127.0.0.1:8081/leaderboard.php");
}
?>


<!DOCTYPE html>
<style>
    :root {
        --graphite: #333333;
        --graphiteD: #212223;
        --pop: #FF3CC7;
        --ice: #00E5E8;
        --ocean: #007C77;
    }
    * {padding: 0;margin: 0;font-family: "Bricolage Grotesque";}
    html, body {
        position: relative;
        z-index: 0;
        height: 100%;
        width: 100%;
    }
    .filter
    {
        position: absolute;
        z-index: 1.;
        background-color: var(--graphiteD);
        opacity: 1.;
        height: 100%;
        width: 100%;
    }

    h1
    {
        text-align: center;
        padding-top: 5%;
        margin-bottom: 40px;
        color: var(--ocean);
    }

    button
    {
        text-transform: uppercase;
    }

    #main
    {
        position: relative;
        z-index: 9999;
    }

    .cnt
    {
        margin: auto;
        background-color: var(--graphite);
        min-height: 520px;
        width: 40%;
        display: flex;
        flex-direction: column;
        border-radius: 10px;
        flex: 1;
    }

    h2
    {
        text-align: center;
        padding-top: 25px;
        margin-bottom: 5px;
    }

    button
    {
        margin: auto auto 20px auto;
        width: 100px;
        padding: 3px 7px;
    }

    .first
    {
        border: 3px solid #eabf12;
        box-shadow: 0 0 20px #eabf12;
        margin-bottom: 25px !important;
    }

    .second
    {
        box-shadow: 0 0 5px #c8c7cc;
    }

    .third
    {
        box-shadow: 0 0 5px #ea7e22;
    }
  
    .user
    {
        background-color: orange;
        margin: 15px 25px 0 25px;
        padding: 10px 25px;
        display: flex;
        background-color: rgba(0, 229, 232, .2);
        justify-content: space-between;
        border-radius: 10px;
    }


</style>


<?php

if (isset($_GET['good']))
{?>
<html>
<head>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,200..800&display=swap" rel="stylesheet">
</head>
<body>
<div class="filter"></div>
<section id="main">
<?php
$good = $_GET['good'];
echo "<h1> " . $good . "/5, ";
if ($good <= 3)
    echo "You are so bad</h1>";
else if ($good <= 4)
    echo "You are not a noob</h1>";
else if ($good == 5)
    echo "BIG BOSS</h1>";
else
    echo "<script>location.href = 'https://www.youtube.com/watch?v=dQw4w9WgXcQ&list=RDdQw4w9WgXcQ&start_radio=1&pp=ygUJcmljayByb2xsoAcB'</script>";
?>
<form method="post" name="adduser" style="display: flex; margin: auto; flex-direction: column;">
    <div style="margin: 0 auto 25px auto; display: flex; justify-content: center;"><input type="text" name="name"></div>
    <div style="margin: auto"><input type="submit" value="Add to last user"></div>
    <input type="text" name="good" value="<?= $good ?>" style="display: none;">
</form>
</section>
<body>
</html>

<?php
die("jpp");
}
?>
<html>
<head>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,200..800&display=swap" rel="stylesheet">
</head>
<body>
<div class="filter"></div>
<section id="main">
    <h1>Are you a noob</h1>
    <div class="cnt">
    <h2 id="question">Leaderboard</h2>
    <div id="responses">
        <?php
        $idx = 0;

        if (isset($_SESSION['user']))
        {
        $mainArray = $_SESSION['user'];
        // Source - https://stackoverflow.com/a/15083608
        // Posted by Athlan
        // Retrieved 2026-07-28, License - CC BY-SA 3.0
    usort($mainArray, function ($a, $b) {
        $a_val = (int) $a['good'];
        $b_val = (int) $b['good'];

        if($a_val > $b_val) return -1;
        if($a_val < $b_val) return 1;
        return 0;
    });
        foreach ($mainArray as $elm)
        {
            if ($idx > 8)
                return ;
            if ($idx == 0)
                echo "<div class='user first'><span>👑</span><span class='username'>" . $elm['name'] . "</span><span>" . $elm['good'] . "</span></div>";
            else if ($idx == 1)
                echo "<div class='user second'><span>🥈</span><span class='username'>" . $elm['name'] . "</span><span>" . $elm['good'] . "</span></div>";
            else if ($idx == 2)
                echo "<div class='user third'><span>🥉</span><span class='username'>" . $elm['name'] . "</span><span>" . $elm['good'] . "</span></div>";
            else
                echo "<div class='user'><span>💩</span><span class='username'>" . $elm['name'] . "</span><span>" . $elm['good'] . "</span></div>";
            $idx++;
        }
        }
        ?>
    </div>
<div style="display: flex;margin-top: auto;">
<button id="reset" style="white-space: nowrap; width: fit-content;background-color: rgba(255, 0, 0, .7);color: white;border:none;cursor: pointer;">Clear session</button>
<button id="new" style="white-space: nowrap; width: fit-content;background-color: rgba(0, 12, 255, .7);color: white;border:none;cursor: pointer;">Retry</button>
</div>
</section>
<body>
</html>
<script>
document.querySelector("#reset").addEventListener("click", _ => location.href="http://127.0.0.1:8082/leaderboard.php?reset=1")
document.querySelector("#new").addEventListener("click", _ => location.href="http://127.0.0.1:8082/index.html")
</script>

