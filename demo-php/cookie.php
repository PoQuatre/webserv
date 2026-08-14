<!DOCTYPE html>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,200..800&display=swap" rel="stylesheet">

<?php
if (session_status() == PHP_SESSION_NONE)
    session_start();
if (isset($_POST['name']) && isset($_POST['val']))
{
    if (!empty($_POST['name']) && !empty($_POST['val']))
    setcookie($_POST['name'], $_POST['val'], time() + 3600);
    header("location: http://127.0.0.1:8088/cookie.php");
}
if (isset($_GET['delete']) && !empty($_GET['delete']))
{
    setcookie($_GET['delete'], 0, time() - 3600);
}
?>
<style>
    :root {
        --graphite: #333333;
        --graphiteD: #212223;
        /*--pop: #FF3CC7;*/
        --pop: #9D44B5;
        --ice: #00E5E8;
        --ocean: #007C77;
    }
    * {padding:0;margin:0;color: white;text-align: center;font-family: "Bricolage Grotesque";}
    html, body {background-color: #0D0F1A;height: 100%;}
    body {display: flex;}
    h2 {color:white;text-align: center;}
    .top{display: flex;flex-direction: column;}
    form{margin: auto;}
    .cookie_lst{display: flex;flex-direction: column;}
    input {color: black;}
    input::placeholder {color: black;}
    section {margin: auto;position: relative;}
    .delete {position: absolute; right: 0;cursor: pointer;}
</style>
<html>
<head></head>
<body>
<style>

</style>
<section>
    <div class="top">
        <h2>Add new cookie</h2>
        <form method="POST">
            <div style="margin-top: 20px;"></div>
            <input type="text" placeholder="name" name="name">
            <div style="margin-top: 10px;"></div>
            <input type="text" placeholder="Value" name="val">
            <div style="margin-top: 10px;"></div>
            <input type="submit" value="add cookie">
        </form>
    </div>
    <div style="margin-top: 50px;"></div>
    <div class="bottom">
        <h2>cookies</h2>
        <div class="cookies_lst">
            <div style="margin-top: 30px;"></div>
            <?php 
        if (!isset($_COOKIE))
        {
            echo "<h2>Pas de cookie</h2>";
        }
        else{
        foreach ($_COOKIE as $key=>$val) {
             ?>
            <div class="cookie_cnt">
                <span class="delete" id="<?= $key ?>">❌</span>
                <h3><?= $key ?></h3>
                <p><?= $val ?></p>
            </div>
<script>
            const <?= $key ?> = document.querySelector("#<?= $key ?>")
                <?= $key ?>.addEventListener("click", _ => {
                console.log("event has been call");
            fetch("http://127.0.0.1:8088/cookie.php?delete=<?= $key ?>")
                .then(_ => location.href = "http://127.0.0.1:8088/cookie.php")
});
</script>
            <div style="margin-top: 10px;"></div>
        <?php }} ?>
        </div>
    </div>
</section>
</body>
</html>
