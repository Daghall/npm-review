"use strict";

const express = require("express");
const fs = require("fs");

const app = express();

const cache = {};

app.use("/v2/search/", (req, res) => {
  const fileName = req.query.q;
  process.stdout.write(`Searching for ${fileName}\n`);
  try {
    const file = cache[fileName] || fs.readFileSync(`./data/${fileName}.json`).toString();
    cache[fileName] = file;
    res.send(file);
  } catch (e) {
    process.stderr.write("Search MISS\n");
    res.send({message: "nothing found"});
  }
});

app.listen(3000);
