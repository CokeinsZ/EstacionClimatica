struct CalidadAire {
  int co2;
  int co;
  int inflamables;
};

struct DatosLocales {
  float tempLocal;
  float humLocal;
  float altLocal;
  float presLocal;
  CalidadAire calidadAire;
};

struct Pronosticos {
  float vientoPronostico;
  float lluviaPronostico;
  float tempPronostico;
};
